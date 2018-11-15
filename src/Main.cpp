#include "Engine.h"
#include "input/Mouse.h"
#include "input/Keyboard.h"

int main(int argc, char* argv[])
{

        Window* window = Engine::Init("../data/shader", "Blue Engine", WINDOWPOSITION_UNDEFINED,
                WINDOWPOSITION_UNDEFINED, 1280, 720, WINDOW_RESIZABLE | WINDOW_BORDERLESS);

		Engine::UnlockFramerate();

        Camera* camera = new Camera(47.0f, 2.0f, 1.0f, 200.0f);
		camera->location = glm::vec3(30.0f, 25.0f, 0.0f);
		camera->rotation = glm::vec2(-3.14f / 2.0f, 0.0f);

		Terrain* terrain = new Terrain(9, 7, 16, 0.5f, 30.0f);

		terrain->Update(camera);
		terrain->Update(camera);
		terrain->Update(camera);

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

		scene->postProcessing->chromaticAberration = new ChromaticAberration(0.7f);

        Mesh* mesh = new Mesh("../data/cube.dae");
        Mesh* sponzaMesh = new Mesh("../data/sponza/sponza.dae");

        Actor* actor = new Actor(mesh);
        Actor* sponza = new Actor(sponzaMesh);
        sponza->modelMatrix = scale(mat4(1.0f), vec3(0.05f));

        MasterRenderer* renderer = new MasterRenderer();

        SceneNode* node = new SceneNode();

		Light* globalLight = new Light(DIRECTIONAL_LIGHT);
		globalLight->direction = vec3(0.0f, -1.0f, 0.5f);
		globalLight->diffuseColor = vec3(253, 194, 109) / 255.0f * 3.0f;
		globalLight->ambient = 0.05f;
		globalLight->AddShadow(new Shadow(125.0f, 0.004f, 2048, 3, 0.7f), camera);		
        globalLight->AddVolumetric(new Volumetric(target->width/2, target->height/2, 20));

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

				globalLight->shadow->Update(camera);

                renderer->RenderScene(window, target, camera, scene);

                window->Update();

        }

        delete camera;
        delete window;

    return 0;

}