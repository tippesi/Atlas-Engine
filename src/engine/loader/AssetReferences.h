#pragma once

#include "../System.h"
#include "../common/SerializationHelper.h"

#include <string>

namespace Atlas {

    namespace Loader {

        /**
         * Converts references from one asset file to other assets. References between files of the same
         * package (a directory containing a package file) are stored relative to the referencing file,
         * such that packages can be moved as a whole. All other references keep their asset directory form.
         * @note Relative references start with "./" or "../". References without one of these prefixes
         * keep their legacy meaning, therefore files written before packages existed load unchanged.
         */
        class AssetReferences {

        public:
            enum class Type {
                Scene = 0,
                Entity,
                Mesh,
                Material
            };

            /**
             * Converts all resource paths of an asset json into references from the owner file.
             * @param j The json of the asset, as written by the corresponding serializer.
             * @param type The type of the asset.
             * @param ownerFile The file the json is written to.
             */
            static void MakeReferences(json& j, Type type, const std::string& ownerFile);

            /**
             * Converts all references of an asset json back into resource paths.
             * @param j The json of the asset, as read from the owner file.
             * @param type The type of the asset.
             * @param ownerFile The file the json was read from.
             */
            static void ResolveReferences(json& j, Type type, const std::string& ownerFile);

            /**
             * Converts a single resource path into a reference from the owner file.
             */
            static std::string MakeReference(const std::string& ownerFile, const std::string& resourcePath);

            /**
             * Converts a single reference from the owner file back into a resource path.
             */
            static std::string ResolveReference(const std::string& ownerFile, const std::string& reference);

            /**
             * Returns the root directory of the package containing the file or an empty string.
             */
            static std::string FindPackageRoot(const std::string& file);

            static const std::string packageFilename;

        };

    }

}
