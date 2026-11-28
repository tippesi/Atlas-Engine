#include <gtest/gtest.h>

#include "Engine.h"
#include "project/Project.h"
#include "scripting/LuaHost.h"
#include "loader/AssetLoader.h"
#include "common/SerializationHelper.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static fs::path MakeTestDirectory(const std::string& name) {

    auto directory = fs::temp_directory_path() / "AtlasEngineTests" / name;
    fs::remove_all(directory);
    fs::create_directories(directory);
    return directory;

}

static void WriteText(const fs::path& path, const std::string& text) {

    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;

}

static json ReadJson(const fs::path& path) {

    std::ifstream stream(path, std::ios::binary);
    return json::parse(stream);

}

TEST(ProjectTest, SavedJsonContainsEngineVersion) {

    auto directory = MakeTestDirectory("Version");

    for (auto format : { Atlas::Loader::JsonFormat::Compact, Atlas::Loader::JsonFormat::MessagePack }) {
        auto path = directory / "file.json";
        json j = { { "value", 1 } };
        ASSERT_TRUE(Atlas::Loader::AssetLoader::WriteJson(path.generic_string(), j, format));

        std::ifstream stream(path, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        auto read = format == Atlas::Loader::JsonFormat::MessagePack ? json::from_msgpack(data) : json::parse(data);
        EXPECT_EQ(read["engineVersion"], Atlas::Engine::GetVersion());
        EXPECT_EQ(read["value"], 1);
    }
    EXPECT_FALSE(Atlas::Engine::GetVersion().empty());

}

TEST(ProjectTest, CreateCopiesStandardDataAndTemplate) {

    auto directory = MakeTestDirectory("Create");

    auto standardData = directory / "data";
    WriteText(standardData / "shader" / "a.csh", "shader");
    WriteText(standardData / "noise.png", "png");
    WriteText(standardData / "local" / "large.bin", "not standard data");

    auto templateDirectory = directory / "templates" / "Empty";
    WriteText(templateDirectory / "Empty.aeproject", R"({ "name": "Empty", "mainScript": "Main.lua", "startScene": "scenes/Main.aescene" })");
    WriteText(templateDirectory / "template.json", R"({ "standardData": [ "shader", "noise.png" ] })");
    WriteText(templateDirectory / "Main.lua", "function Update(deltaTime) end");
    WriteText(templateDirectory / ".atlas" / "config.json", "{}");

    auto project = Atlas::Project::Create((directory / "projects").generic_string(), "MyGame",
        templateDirectory.generic_string(), standardData.generic_string());
    ASSERT_NE(project, nullptr);

    auto root = directory / "projects" / "MyGame";
    EXPECT_TRUE(fs::exists(root / "MyGame.aeproject"));
    EXPECT_TRUE(fs::exists(root / "Main.lua"));
    EXPECT_TRUE(fs::exists(root / "shader" / "a.csh"));
    EXPECT_TRUE(fs::exists(root / "noise.png"));
    EXPECT_FALSE(fs::exists(root / "local"));
    EXPECT_FALSE(fs::exists(root / ".atlas"));
    EXPECT_FALSE(fs::exists(root / "template.json"));
    EXPECT_FALSE(fs::exists(root / "Empty.aeproject"));

    auto saved = ReadJson(root / "MyGame.aeproject");
    EXPECT_EQ(saved["name"], "MyGame");
    EXPECT_EQ(saved["mainScript"], "Main.lua");
    EXPECT_EQ(saved["engineVersion"], Atlas::Engine::GetVersion());

    auto loaded = Atlas::Project::Load((root / "MyGame.aeproject").generic_string());
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->startScene, "scenes/Main.aescene");
    EXPECT_EQ(loaded->engineVersion, Atlas::Engine::GetVersion());
    EXPECT_EQ(fs::path(loaded->GetAssetDirectory()), fs::absolute(root).lexically_normal());

    // A second project with the same name would overwrite the first one
    EXPECT_EQ(Atlas::Project::Create((directory / "projects").generic_string(), "MyGame",
        templateDirectory.generic_string(), standardData.generic_string()), nullptr);

}

TEST(ProjectTest, AssetDirectoryIsRelativeToTheProjectFile) {

    auto directory = MakeTestDirectory("AssetDirectory");
    WriteText(directory / "Game.aeproject", R"({ "name": "Game", "assetDirectory": "data" })");

    auto project = Atlas::Project::Load((directory / "Game.aeproject").generic_string());
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(fs::path(project->GetAssetDirectory()), fs::absolute(directory / "data").lexically_normal());

    EXPECT_EQ(Atlas::Project::Load((directory / "Missing.aeproject").generic_string()), nullptr);

}

TEST(ProjectTest, EmptyTemplateListsExistingEngineData) {

    auto engineDirectory = fs::path(__FILE__).parent_path().parent_path().parent_path();
    auto templateFile = engineDirectory / "templates" / "Empty" / "template.json";
    ASSERT_TRUE(fs::exists(templateFile));

    auto j = ReadJson(templateFile);
    ASSERT_TRUE(j.contains("standardData"));
    for (const auto& path : j["standardData"])
        EXPECT_TRUE(fs::exists(engineDirectory / "data" / path.get<std::string>())) << path;

    EXPECT_NE(Atlas::Project::Load((engineDirectory / "templates" / "Empty" / "Empty.aeproject").generic_string()), nullptr);
    EXPECT_NE(Atlas::Project::Load((engineDirectory / "AtlasEngine.aeproject").generic_string()), nullptr);

}

TEST(ProjectTest, LuaHostRunsTheMainScript) {

    auto directory = MakeTestDirectory("LuaHost");
    auto scriptPath = directory / "Main.lua";
    WriteText(scriptPath, R"(
loaded = false
updates = 0
time = 0
function Load()
    loaded = true
    projectName = Project.name
end
function Update(deltaTime)
    updates = updates + 1
    time = time + deltaTime
    if updates == 2 then
        Host.LoadScene("scenes/Next.aescene")
    end
end
)");

    // Scripts see the activated project
    Atlas::Project::active = Atlas::CreateRef<Atlas::Project>();
    Atlas::Project::active->name = "Test";

    Atlas::Scripting::LuaHost host;
    Atlas::Project::active.reset();
    ASSERT_TRUE(host.Load(scriptPath.generic_string()));

    auto& state = host.GetState();
    EXPECT_TRUE(state["loaded"].get<bool>());
    EXPECT_EQ(state["projectName"].get<std::string>(), "Test");

    host.Update(0.5f);
    EXPECT_TRUE(host.requestedScene.empty());
    host.Update(0.25f);
    EXPECT_EQ(state["updates"].get<int32_t>(), 2);
    EXPECT_FLOAT_EQ(state["time"].get<float>(), 0.75f);
    EXPECT_EQ(host.requestedScene, "scenes/Next.aescene");
    EXPECT_FALSE(host.HasFailed());

}

TEST(ProjectTest, LuaHostStopsAfterAnError) {

    auto directory = MakeTestDirectory("LuaHostError");
    auto scriptPath = directory / "Main.lua";
    WriteText(scriptPath, R"(
updates = 0
function Update(deltaTime)
    updates = updates + 1
    error("broken")
end
)");

    Atlas::Scripting::LuaHost host;
    ASSERT_TRUE(host.Load(scriptPath.generic_string()));
    host.Update(0.1f);
    host.Update(0.1f);
    EXPECT_TRUE(host.HasFailed());
    EXPECT_EQ(host.GetState()["updates"].get<int32_t>(), 1);

}
