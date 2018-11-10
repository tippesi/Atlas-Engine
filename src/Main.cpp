#include "Engine.h"
#include "input/Mouse.h"
#include "input/Keyboard.h"

int main(int argc, char* argv[])
{

		// This has to be set before initialization
		ShaderSource::SetSourceDirectory("../data/shader");

        Window* window = Engine::Init("Blue Engine", WINDOWPOSITION_UNDEFINED, WINDOWPOSITION_UNDEFINED,
                1280, 720, WINDOW_RESIZABLE);

		GeometryRenderer::InitShaderBatch("deferred/geometry.vsh", "deferred/geometry.fsh");
		ShadowRenderer::InitShaderBatch("shadowmapping.vsh", "shadowmapping.fsh");

        Camera* camera = new Camera(45.0f, 2.0f, 1.0f, 400.0f);
		camera->location = vec3(51.323f, 0.510f, -9.508);
		camera->rotation = vec2(5.059f, 0.345f);

        Texture* texture = new Texture("../data/image.png");

        window->SetIcon(texture);

        RenderTarget* target = new RenderTarget(1920, 1080);

        Scene* scene = new Scene();

        Cubemap* cubemap = new Cubemap("../data/cubemap/right.png",
                "../data/cubemap/left.png",
                "../data/cubemap/top.png",
                "../data/cubemap/bottom.png",
                "../data/cubemap/front.png",
                "../data/cubemap/back.png");

        scene->sky->skybox = new Skybox(cubemap);

        Mesh* mesh = new Mesh("../data/cube.dae");
        Mesh* sponzaMesh = new Mesh("../data/sponza/sponza.dae");

        Actor* actor = new Actor(mesh);
        Actor* sponza = new Actor(sponzaMesh);
        sponza->modelMatrix = scale(mat4(1.0f), vec3(0.05f));

        MasterRenderer* renderer = new MasterRenderer();

        SceneNode* node = new SceneNode();

		Light* globalLight = new Light(DIRECTIONAL_LIGHT);
		globalLight->direction = vec3(0.0f, -1.0f, 0.0f);
		globalLight->ambient = 0.1f;
		globalLight->shadow = new Shadow(50.0f, 0.001f, 1);

        node->Add(actor);
        scene->rootNode->Add(node);

		scene->Add(sponza);

		scene->Add(globalLight);

        node->transformationMatrix = translate(vec3(0.0f, 1.0f, 5.0f));

		// We create the controller handler
		MouseHandler* mouseHandler = CreateMouseHandler(camera, 1.5f, 0.25f);
		mouseHandler->lock = true;
		KeyboardHandler* keyboardHandler = CreateKeyboardHandler(camera, 7.0f, 0.3f);

        // For now we will leave the main loop here until we implement a more advanced event system
        // Our event structure
        SDL_Event event;
        bool quit = false;

        // We need the time passed per frame in the rendering loop
        unsigned int time = 0;

        // Handle events and rendering here
        while (!quit) {

                unsigned int deltatime = SDL_GetTicks() - time;
                time = SDL_GetTicks();

                // Poll all the events
                while (SDL_PollEvent(&event)) {

                        if (event.type == SDL_QUIT) {

                                // If the SDL event is telling us to quit we should do it
                                quit = true;

                        }
                        else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {

                                // If the user presses escape we also want to quit
                                quit = true;

                        }
						else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {

							// If the user presses escape we also want to quit
							quit = true;

						}
						else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {

							mouseHandler->lock = false;

						}
						else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {

							mouseHandler->lock = true;

						}

				}

				CalculateMouseHandler(mouseHandler, camera, deltatime);
				CalculateKeyboardHandler(keyboardHandler, camera, deltatime);

                camera->UpdateView();
                camera->UpdateProjection();

                scene->rootNode->transformationMatrix = glm::rotate((float)time / 1000.0f, vec3(0.0f, 1.0f, 0.0f));
                actor->modelMatrix = glm::rotate((float)time / 500.0f, vec3(0.0f, 1.0f, 0.0f));

                scene->Update();

				// globalLight->shadow->Update(camera);

                renderer->RenderScene(window, target, camera, scene);

                window->Update();

        }

        delete camera;
        delete window;

    return 0;

}

