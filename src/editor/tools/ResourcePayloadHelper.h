#pragma once

#include "../FileImporter.h"

#include "resource/ResourceManager.h"

namespace Atlas::Editor {

	class ResourcePayloadHelper {

	public:
		template<class T>
		static ResourceHandle<T> AcceptDropResource() {

            ResourceHandle<T> handle;

            if (ImGui::BeginDragDropTarget()) {
                auto dropPayload = ImGui::GetDragDropPayload();

                bool compatible = false;

                std::string resourcePath;
                if (dropPayload->IsDataType("ContentBrowserResource")) {
                    // Pretty dangerous
                    resourcePath = std::string(reinterpret_cast<const char*>(dropPayload->Data));

                    compatible = FileImporter::AreCompatible<T>(resourcePath);
                }                

                if (compatible && ImGui::AcceptDragDropPayload("ContentBrowserResource")) {
                    handle = FileImporter::ImportFile<T>(resourcePath);
                }

                ImGui::EndDragDropTarget();
            }

            return handle;

		}

        template<class T>
		static std::string AcceptDropResourceAndGetPath() {

            std::string path;

            if (ImGui::BeginDragDropTarget()) {
                auto dropPayload = ImGui::GetDragDropPayload();

                bool compatible = false;

                std::string resourcePath;
                if (dropPayload->IsDataType("ContentBrowserResource")) {
                    resourcePath.resize(dropPayload->DataSize);

                    // Pretty dangerous
                    resourcePath = std::string(reinterpret_cast<const char*>(dropPayload->Data));

                    compatible = FileImporter::AreCompatible<T>(resourcePath);
                }                

                if (compatible && ImGui::AcceptDragDropPayload("ContentBrowserResource")) {
                    path = resourcePath;
                }

                ImGui::EndDragDropTarget();
            }

            return path;

		}

	};

}