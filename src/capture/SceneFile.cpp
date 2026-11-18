#include "SceneFile.h"
#include "Process.h"

#include "Log.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace Atlas::Capture {

    static std::vector<uint8_t> ReadBytes(const fs::path& path) {

        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open())
            throw std::runtime_error("Couldn't open " + path.string());
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {});

    }

    static bool IsJsonFile(const fs::path& path) {

        return path.extension() == ".json";

    }

    // Collects the entities while the parser walks the file, without building the document
    class EntityReader : public nlohmann::json_sax<json> {

    public:
        EntityReader(std::vector<FileEntity>& entities, bool rootIsEntity, bool namesOnly)
            : entities(entities), rootIsEntity(rootIsEntity), namesOnly(namesOnly) {}

        bool null() override { return true; }
        bool boolean(bool) override { return true; }
        bool number_integer(number_integer_t value) override { return Integer(int64_t(value)); }
        bool number_unsigned(number_unsigned_t value) override { return Integer(int64_t(value)); }
        bool number_float(number_float_t value, const string_t&) override { return Number(float(value)); }

        bool binary(binary_t& value) override {

            // Binary scenes store matrices as raw floats
            const auto& frame = frames.back();
            if (frame.role == Role::Transform && frame.key == "matrix" && value.size() == sizeof(mat4)) {
                auto& values = matrices[frame.entity];
                values.resize(16);
                std::memcpy(values.data(), value.data(), sizeof(mat4));
            }
            // Scenes with binary columns store the index lists and the columns as raw values
            else if (frame.role == Role::EntityTable && frame.key == "parent") {
                parents = Decode<int32_t>(value);
            }
            else if (frame.role == Role::Section && frame.key == "entity") {
                for (auto entity : Decode<uint32_t>(value))
                    AddSectionEntity(entity);
            }
            else if (frame.role == Role::Columns) {
                // The columns come before the entities of their section
                columns[frame.key] = std::move(static_cast<std::vector<uint8_t>&>(value));
            }
            return true;

        }

        bool string(string_t& value) override {

            auto& frame = frames.back();
            if (frame.role == Role::Name && frame.key == "name")
                entities[frame.entity].name = value;
            else if (frame.role == Role::Mesh && frame.key == "resourcePath")
                entities[frame.entity].mesh = value;
            else if (frame.role == Role::Resources)
                resources.push_back(value);
            return true;

        }

        bool start_object(std::size_t) override {

            if (frames.empty()) {
                frames.push_back({ rootIsEntity ? Role::Entity : Role::Root, {}, rootIsEntity ? AddEntity(-1) : -1 });
                return true;
            }

            const auto parent = frames.back();
            auto role = Role::Ignored;
            auto entity = parent.entity;
            if (parent.role == Role::Entities) {
                role = Role::Entity;
                entity = AddEntity(parent.entity);
            }
            else if (parent.role == Role::Entity) {
                if (parent.key == "name")
                    role = Role::Name;
                else if (parent.key == "transform" && !namesOnly)
                    role = Role::Transform;
                else if (parent.key == "mesh" && !namesOnly)
                    role = Role::Mesh;
            }
            else if (parent.role == Role::Matrix) {
                // Older files store a matrix as one object per column
                role = Role::Matrix;
            }
            else if (parent.role == Role::Root && parent.key == "entities") {
                role = Role::EntityTable;
            }
            else if (parent.role == Role::Root && parent.key == "components") {
                role = Role::Components;
            }
            else if (parent.role == Role::Components) {
                role = Role::Section;
                section = parent.key;
                sectionEntities.clear();
                sectionValue = 0;
                columns.clear();
            }
            else if (parent.role == Role::Section && parent.key == "columns") {
                role = Role::Columns;
            }
            else if (parent.role == Role::SectionValues) {
                // The entities of a section come before its values
                entity = int32_t(sectionEntities.at(sectionValue++));
                if (section == "name")
                    role = Role::Name;
                else if (section == "transform" && !namesOnly)
                    role = Role::Transform;
                else if (section == "mesh" && !namesOnly)
                    role = Role::Mesh;
            }
            frames.push_back({ role, {}, entity });
            return true;

        }

        bool key(string_t& value) override {

            auto& frame = frames.back();
            frame.key = value;
            if (frame.role == Role::Entity && !namesOnly && value != "id" && value != "entities" && value != "root" && value != "name" &&
                value != "transform")
                entities[frame.entity].components.push_back(value);
            return true;

        }

        bool end_object() override {

            if (frames.back().role == Role::Section && !columns.empty())
                ApplyColumns();
            frames.pop_back();
            return true;

        }

        bool start_array(std::size_t) override {

            if (frames.empty()) {
                frames.push_back({ Role::Ignored, {}, -1 });
                return true;
            }

            const auto& parent = frames.back();
            auto role = Role::Ignored;
            if ((parent.role == Role::Root || parent.role == Role::Entity) && parent.key == "entities")
                role = Role::Entities;
            else if ((parent.role == Role::Transform && parent.key == "matrix") || parent.role == Role::Matrix)
                role = Role::Matrix;
            else if (parent.role == Role::Root && parent.key == "resources")
                role = Role::Resources;
            else if (parent.role == Role::EntityTable && parent.key == "parent")
                role = Role::Parents;
            else if (parent.role == Role::Section && parent.key == "entity")
                role = Role::SectionEntities;
            else if (parent.role == Role::Section && parent.key == "values")
                role = Role::SectionValues;
            frames.push_back({ role, {}, parent.entity });
            return true;

        }

        bool end_array() override {

            frames.pop_back();
            return true;

        }

        bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception& exception) override {

            throw std::runtime_error("Invalid file at byte " + std::to_string(position) + ": " + exception.what());

        }

        std::vector<std::vector<float>> matrices;

        // Binary scenes reference meshes by their index in the resource table
        std::vector<std::string> resources;
        std::unordered_map<int32_t, size_t> meshIndices;

        // Scenes store the parents in their entity table, the components in a section per component type
        std::vector<int32_t> parents;

    private:
        enum class Role {
            Root,
            Entities,
            Entity,
            Name,
            Transform,
            Matrix,
            Mesh,
            Resources,
            EntityTable,
            Parents,
            Components,
            Section,
            SectionEntities,
            SectionValues,
            Columns,
            Ignored
        };

        struct Frame {

            Role role;
            std::string key;
            int32_t entity;

        };

        int32_t AddEntity(int32_t parent) {

            entities.push_back({ .parent = parent });
            if (!namesOnly)
                matrices.emplace_back();
            return int32_t(entities.size()) - 1;

        }

        bool Number(float value) {

            const auto& frame = frames.back();
            if (frame.role == Role::Matrix)
                matrices[frame.entity].push_back(value);
            return true;

        }

        bool Integer(int64_t value) {

            const auto& frame = frames.back();
            if (frame.role == Role::Mesh && frame.key == "resourcePath")
                meshIndices[frame.entity] = size_t(value);
            else if (frame.role == Role::Parents)
                parents.push_back(int32_t(value));
            else if (frame.role == Role::SectionEntities)
                AddSectionEntity(size_t(value));
            return Number(float(value));

        }

        template<class T>
        static std::vector<T> Decode(const std::vector<uint8_t>& data) {

            std::vector<T> values(data.size() / sizeof(T));
            std::memcpy(values.data(), data.data(), values.size() * sizeof(T));
            return values;

        }

        void ApplyColumns() {

            // Files that don't match their entity list are reported, they would otherwise be read out of bounds
            auto count = sectionEntities.size();
            auto hasCount = [&](const char* name, size_t elementSize, size_t expected) {
                if (columns[name].size() == expected * elementSize)
                    return true;
                Log::Error("Scene column " + section + "." + name + " doesn't match its entities");
                return false;
            };

            if (section == "name" && hasCount("offset", sizeof(uint32_t), count + 1)) {
                auto offsets = Decode<uint32_t>(columns["offset"]);
                const auto& text = columns["text"];
                if (offsets.back() > text.size() || !std::is_sorted(offsets.begin(), offsets.end())) {
                    Log::Error("Scene name column has invalid offsets");
                    columns.clear();
                    return;
                }
                for (size_t i = 0; i < sectionEntities.size(); i++)
                    entities[sectionEntities[i]].name.assign(reinterpret_cast<const char*>(text.data()) + offsets[i],
                        offsets[i + 1] - offsets[i]);
            }
            else if (section == "transform" && !namesOnly && hasCount("matrix", sizeof(mat4), count)) {
                auto values = Decode<float>(columns["matrix"]);
                for (size_t i = 0; i < sectionEntities.size(); i++)
                    matrices[sectionEntities[i]].assign(values.begin() + i * 16, values.begin() + (i + 1) * 16);
            }
            else if (section == "mesh" && !namesOnly && hasCount("resource", sizeof(uint32_t), count)) {
                auto indices = Decode<uint32_t>(columns["resource"]);
                for (size_t i = 0; i < sectionEntities.size(); i++)
                    if (indices[i] != UINT32_MAX)
                        meshIndices[int32_t(sectionEntities[i])] = indices[i];
            }
            columns.clear();

        }

        void AddSectionEntity(size_t entity) {

            // Sections can come before the entity table
            if (entity >= entities.size()) {
                entities.resize(entity + 1);
                if (!namesOnly)
                    matrices.resize(entity + 1);
            }
            sectionEntities.push_back(entity);
            if (!namesOnly && section != "name" && section != "transform" && section != "hierarchy")
                entities[entity].components.push_back(section);

        }

        std::vector<FileEntity>& entities;
        std::vector<Frame> frames;
        bool rootIsEntity;
        bool namesOnly;

        std::string section;
        std::vector<size_t> sectionEntities;
        size_t sectionValue = 0;
        std::unordered_map<std::string, std::vector<uint8_t>> columns;

    };

    static std::vector<FileEntity> ReadEntities(const fs::path& path, bool namesOnly) {

        auto bytes = ReadBytes(path);
        std::vector<FileEntity> entities;
        EntityReader reader(entities, path.extension() == ".aeprefab", namesOnly);
        if (IsJsonFile(path))
            json::sax_parse(bytes.begin(), bytes.end(), &reader, json::input_format_t::json, true, true);
        else
            json::sax_parse(bytes.begin(), bytes.end(), &reader, json::input_format_t::msgpack);

        for (const auto& [entity, index] : reader.meshIndices)
            entities[entity].mesh = reader.resources.at(index);

        // Parents come before their children in the entity table
        if (entities.size() < reader.parents.size()) {
            entities.resize(reader.parents.size());
            reader.matrices.resize(namesOnly ? 0 : entities.size());
        }
        for (size_t i = 0; i < reader.parents.size(); i++)
            entities[i].parent = reader.parents[i];

        if (namesOnly)
            return entities;

        for (size_t i = 0; i < entities.size(); i++) {
            auto& entity = entities[i];
            const auto& values = reader.matrices[i];
            if (values.size() == 16)
                entity.local = glm::make_mat4(values.data());
            entity.global = entity.parent >= 0 ? entities[entity.parent].global * entity.local : entity.local;
        }
        return entities;

    }

    std::vector<FileEntity> ReadFileEntities(const fs::path& path) {

        return ReadEntities(path, false);

    }

    std::vector<std::string> ReadEntityNames(const fs::path& path) {

        std::error_code error;
        auto size = fs::file_size(path, error);
        // The count is a 128 bit integer on macOS, which std::to_string doesn't take
        auto time = static_cast<int64_t>(fs::last_write_time(path, error).time_since_epoch().count());
        auto key = std::hash<std::string>()(fs::absolute(path).generic_string() + "|" + std::to_string(size) + "|" + std::to_string(time));
        auto cache = GetExecutablePath().parent_path() / ".cache" / "capture-names" / (path.stem().string() + "-" + std::to_string(key) + ".txt");

        std::vector<std::string> names;
        if (std::ifstream stream(cache); stream.is_open()) {
            for (std::string line; std::getline(stream, line);)
                names.push_back(line);
            return names;
        }

        for (auto& entity : ReadEntities(path, true))
            names.push_back(std::move(entity.name));

        fs::create_directories(cache.parent_path(), error);
        // Older caches of the same file are of no use anymore
        for (const auto& entry : fs::directory_iterator(cache.parent_path(), error))
            if (entry.path().filename().string().starts_with(path.stem().string() + "-"))
                fs::remove(entry.path(), error);
        std::string content;
        for (const auto& name : names)
            content += name + "\n";
        WriteFileAtomic(cache, content);
        return names;

    }

    static vec3 ToVector(const json& j) {

        if (j.is_array() && j.size() >= 3)
            return vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
        if (j.is_object())
            return vec3(j.value("x", 0.0f), j.value("y", 0.0f), j.value("z", 0.0f));
        return vec3(0.0f);

    }

    MeshFileInfo ReadMeshFile(const fs::path& path) {

        auto bytes = ReadBytes(path);
        json j;
        try {
            j = json::from_msgpack(bytes);
        }
        catch (const json::exception&) {
            j = json::parse(bytes.begin(), bytes.end());
        }

        MeshFileInfo info;
        info.name = j.value("name", path.stem().string());
        info.impostor = j.value("impostor", std::string());

        const auto& data = j.value("data", json::object());
        if (data.contains("aabb"))
            info.bounds = Volume::AABB(ToVector(data["aabb"]["min"]), ToVector(data["aabb"]["max"]));
        info.vertexCount = data.value("vertexCount", int64_t(0));
        info.indexCount = data.value("indexCount", int64_t(0));
        for (const auto& material : data.value("materials", json::array()))
            info.materials.push_back(material.is_string() ? material.get<std::string>() : material.dump());
        for (const auto& sub : data.value("subData", json::array())) {
            info.subData.emplace_back(sub.value("name", std::string()), sub.value("materialIdx", 0));
            const auto& lods = sub.value("lods", json::array());
            info.lodTriangles.resize(std::max(info.lodTriangles.size(), lods.size()));
            for (size_t i = 0; i < lods.size(); i++)
                info.lodTriangles[i] += lods[i][1].get<int64_t>() / 3;
        }

        for (const auto& [key, value] : j.items())
            if (key != "data" && key != "impostor" && key != "name" && !value.is_object() && !value.is_array())
                info.settings[key] = value;

        return info;

    }

    static size_t EditDistance(const std::string& a, const std::string& b) {

        std::vector<size_t> row(b.size() + 1);
        for (size_t j = 0; j <= b.size(); j++)
            row[j] = j;

        for (size_t i = 1; i <= a.size(); i++) {
            size_t diagonal = row[0];
            row[0] = i;
            for (size_t j = 1; j <= b.size(); j++) {
                auto above = row[j];
                auto same = std::tolower(static_cast<unsigned char>(a[i - 1])) == std::tolower(static_cast<unsigned char>(b[j - 1]));
                row[j] = std::min({ row[j] + 1, row[j - 1] + 1, diagonal + (same ? 0 : 1) });
                diagonal = above;
            }
        }
        return row[b.size()];

    }

    std::vector<std::string> FindClosestNames(const std::vector<std::string>& names, const std::string& text, size_t count) {

        // Wildcards don't count as differences
        std::string pattern;
        std::copy_if(text.begin(), text.end(), std::back_inserter(pattern), [](char c) { return c != '*' && c != '?'; });

        std::vector<std::pair<size_t, std::string>> scored;
        std::vector<std::string> unique = names;
        std::sort(unique.begin(), unique.end());
        unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
        for (const auto& name : unique) {
            // A name that contains the text is as close as it gets
            auto lowerName = name, lowerPattern = pattern;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            auto distance = lowerName.find(lowerPattern) != std::string::npos ? 0 : EditDistance(lowerName, lowerPattern);
            scored.emplace_back(distance, name);
        }
        std::partial_sort(scored.begin(), scored.begin() + std::min(count, scored.size()), scored.end());

        std::vector<std::string> closest;
        for (size_t i = 0; i < std::min(count, scored.size()); i++)
            closest.push_back(scored[i].second);
        return closest;

    }

}
