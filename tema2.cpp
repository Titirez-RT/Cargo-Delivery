#include "lab_m1/tema2/tema2.h"

#include <vector>
#include <string>
#include <iostream>
#include <math.h>
#include <queue>
#include <ctime>
#include <cstdlib>

using namespace std;
using namespace m1;


Tema2::Tema2()
{
}


Tema2::~Tema2()
{
}


void Tema2::Init()
{
    renderCameraTarget = false;

	// set camera above the scene
    camera = new implemented::Camera();
    camera->Set(glm::vec3(0, 25, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));

    CreatePlain();
	CreateMountains();
    CreateRivers();

    // create central station - cube
    CreateParallelepiped(1, 1, 1, central_station_color, "central_station");

	// create other stations
	CreateParallelepiped(3, 1, 1, station_1_color, "station_1");
	CreateCylinder(1, 0.5, station_2_color, "station_2");
    CreateCone(1, 0.5, station_3_color, "station_3");
	CreatePyramid(1, 1, station_4_color, "station_4");

    // arrow to point to next rail
	CreateArrow();

	// create railway
	CreateRailway();
    DrawRails();
    AddBranches();

	// initialize train and wagon
    train.currentRailIndex = 21;
    train.progress = 0.27f;
    train.speed = 3.0f;
    wagon.railQueue.push(21);
    wagon.progress = 0;
    wagon.speed = 3.0f;

    CreateTrain();
    CreateWagon();

    end_game = false;
    timer = 0.0f;
	timer1 = respawn_time;
	timer2 = respawn_time;
    timer3 = respawn_time;
    timer4 = respawn_time;
    
	srand(time(NULL));

    RandomizeObjects();

	// create viewport for mini-map
    glm::ivec2 resolution = window->GetResolution();
	float square = min(resolution.x, resolution.y);
    miniViewportArea = ViewportArea(resolution.x - square / 2.5f, resolution.y - square / 2.5f, square / 2.5f, square / 2.5f);

    fov = 90.0f;
    projectionMatrix = glm::perspective(RADIANS(fov), window->props.aspectRatio, 0.01f, 200.0f);

	// text renderer for end game screen
    textRenderer = new gfxc::TextRenderer(window->props.selfDir, resolution.x, resolution.y);
    textRenderer->Load("assets/fonts/Hack-Bold.ttf", 50);

	// shader for central station
    {
        Shader* shader = new Shader("central_station");
        shader->AddShader(PATH_JOIN(window->props.selfDir, SOURCE_PATH::M1, "tema2", "shaders", "VertexShader.glsl"), GL_VERTEX_SHADER);
        shader->AddShader(PATH_JOIN(window->props.selfDir, SOURCE_PATH::M1, "tema2", "shaders", "FragmentShader.glsl"), GL_FRAGMENT_SHADER);
        shader->CreateAndLink();
        shaders[shader->GetName()] = shader;
    }
}


void Tema2::FrameStart()
{
    // Clears the color buffer (using the previously set color) and depth buffer
    glClearColor(0.52f, 0.8f, 0.92f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::ivec2 resolution = window->GetResolution();
    // Sets the screen area where to draw
    glViewport(0, 0, resolution.x, resolution.y);
}


void Tema2::Update(float deltaTimeSeconds)
{
    timer += deltaTimeSeconds;
	timer1 += deltaTimeSeconds;
    timer2 += deltaTimeSeconds;
    timer3 += deltaTimeSeconds;
    timer4 += deltaTimeSeconds;

	// check end game
    if (timer >= time_to_complete_mission) {
        DrawEndGame();
        end_game = true;
        camera->Set(glm::vec3(0, 25, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
        cameraFollowing = false;
        return;
    }

    PickUpLogic();
    DoneMission();

    // draw the scene
    glm::ivec2 resolution = window->GetResolution();
    glViewport(0, 0, resolution.x, resolution.y);
    DrawAllScene(deltaTimeSeconds);

	// generate the mini-map if switched to train view
    if (cameraFollowing) {
        glClear(GL_DEPTH_BUFFER_BIT);
        glViewport(miniViewportArea.x, miniViewportArea.y, miniViewportArea.width, miniViewportArea.height);

        glm::vec3 cameraOldPos = camera->position;
        glm::vec3 cameraOldForward = camera->forward;
        glm::vec3 cameraOldUp = camera->up;

        Rail r = rails[train.currentRailIndex];
        r = rails[train.currentRailIndex];
        glm::vec3 pos = glm::mix(r.startPosition, r.endPosition, train.progress);
        glm::mat4 oldProjectionMatrix = projectionMatrix;
        projectionMatrix = glm::perspective(RADIANS(fov), 1.0f, 0.01f, 200.0f);

		// set camera above the train and generate the mini-map
        camera->Set(pos + glm::vec3(0, 10, 0), pos, glm::vec3(0, 0, -1));
        DrawAllScene(0);
        camera->Set(cameraOldPos, cameraOldPos + cameraOldForward, cameraOldUp);
        projectionMatrix = oldProjectionMatrix;
    }

	// update camera position to follow the train
    if (cameraFollowing) {
        Rail r = rails[train.currentRailIndex];
        glm::vec3 pos = glm::mix(r.startPosition, r.endPosition, train.progress);
        glm::vec3 dir = r.endPosition - r.startPosition;
        camera->position = pos - camera->forward * cameraOffset + glm::vec3(0, cameraOffsetY, 0);
    }
}


void Tema2::FrameEnd()
{
    //DrawCoordinateSystem(camera->GetViewMatrix(), projectionMatrix);
}


void Tema2::RenderMesh(Mesh * mesh, Shader * shader, const glm::mat4 & modelMatrix)
{
    if (!mesh || !shader || !shader->program)
        return;

    // Render an object using the specified shader and the specified position
    shader->Use();
    glUniformMatrix4fv(shader->loc_view_matrix, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix()));
    glUniformMatrix4fv(shader->loc_projection_matrix, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
    glUniformMatrix4fv(shader->loc_model_matrix, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    // send info to shader about time and how to generate the color
    int location = glGetUniformLocation(shader->program, "TimeInfo");
    glUniform2f(location, timer, time_to_complete_mission);
    mesh->Render();
}

void Tema2::OnInputUpdate(float deltaTime, int mods)
{
    // move the camera only if MOUSE_RIGHT button is pressed
    //if (window->MouseHold(GLFW_MOUSE_BUTTON_RIGHT))
    //{
    //    float cameraSpeed = 2.0f;

    //    if (window->KeyHold(GLFW_KEY_W)) {
    //        // TODO(student): Translate the camera forward
    //        float distance = cameraSpeed * deltaTime * 5;
    //        camera->TranslateForward(distance);
    //    }

    //    if (window->KeyHold(GLFW_KEY_A)) {
    //        // TODO(student): Translate the camera to the left
    //        float distance = -cameraSpeed * deltaTime;
    //        camera->TranslateRight(distance);
    //    }

    //    if (window->KeyHold(GLFW_KEY_S)) {
    //        // TODO(student): Translate the camera backward
    //        float distance = -cameraSpeed * deltaTime;
    //        camera->TranslateForward(distance);
    //    }

    //    if (window->KeyHold(GLFW_KEY_D)) {
    //        // TODO(student): Translate the camera to the right
    //        float distance = cameraSpeed * deltaTime;
    //        camera->TranslateRight(distance);
    //    }

    //    if (window->KeyHold(GLFW_KEY_Q)) {
    //        // TODO(student): Translate the camera downward
    //        float distance = -cameraSpeed * deltaTime;
    //        camera->TranslateUpward(distance);
    //    }

    //    if (window->KeyHold(GLFW_KEY_E)) {
    //        // TODO(student): Translate the camera upward
    //        float distance = cameraSpeed * deltaTime;
    //        camera->TranslateUpward(distance);
    //    }
    //}

    // TODO(student): Change projection parameters. Declare any extra
    // variables you might need in the class header. Inspect this file
    // for any hardcoded projection arguments (can you find any?) and
    // replace them with those extra variables.

    /*if (window->KeyHold(GLFW_KEY_Z)) {
        fov -= 50.0f * deltaTime;
        if (fov < 1.0f) {
            fov = 1.0f;
        }
        glm::ivec2 resolution = window->GetResolution();
        float aspectRatio = (float)resolution.x / (float)resolution.y;
        projectionMatrix = glm::perspective(RADIANS(fov), aspectRatio, 0.01f, 200.0f);
    }

    if (window->KeyHold(GLFW_KEY_X)) {
        fov += 50.0f * deltaTime;
        if (fov > 120.0f) {
            fov = 120.0f;
        }
        glm::ivec2 resolution = window->GetResolution();
        float aspectRatio = (float)resolution.x / (float)resolution.y;
        projectionMatrix = glm::perspective(RADIANS(fov), aspectRatio, 0.01f, 200.0f);
    }*/
}

void Tema2::OnKeyPress(int key, int mods)
{
    // choose rail at junction
    if (train.progress >= 1.0f) {
        if (window->KeyHold(GLFW_KEY_A))
            junctionIndex--;
        if (window->KeyHold(GLFW_KEY_D))
            junctionIndex++;
        if (junctionIndex < 0)
            junctionIndex = rails[train.currentRailIndex].branches_end.size() - 1;
        if (junctionIndex >= rails[train.currentRailIndex].branches_end.size())
            junctionIndex = 0;

        if (window->KeyHold(GLFW_KEY_ENTER)) {
            train.progress = 0.0f;
            gap = 0;
            int next = rails[train.currentRailIndex].branches_end[junctionIndex];
            if (rails[train.currentRailIndex].endPosition != rails[next].startPosition) {
                glm::vec3 aux = rails[next].startPosition;
                rails[next].startPosition = rails[next].endPosition;
                rails[next].endPosition = aux;

                vector<int> temp = rails[next].branches_end;
                rails[next].branches_end = rails[next].branches_start;
                rails[next].branches_start = temp;
            }
            train.currentRailIndex = next;
			junctionIndex = 0;
            wagon.railQueue.push(next);
        }
    }

    if (end_game == true && key == GLFW_KEY_R) {
		// reset everything
        end_game = false;

        timer = 0.0f;
        timer1 = respawn_time;
        timer2 = respawn_time;
        timer3 = respawn_time;
        timer4 = respawn_time;

        missions = 0;
        station_1_items = 0;
        station_2_items = 0;
        station_3_items = 0;
		station_4_items = 0;

        RandomizeObjects();

        train.currentRailIndex = 21;
        train.progress = 0.27f;
        wagon.railQueue = std::queue<int>();
        wagon.railQueue.push(21);
        wagon.progress = 0;
    }

    if (key == GLFW_KEY_V && end_game == false) {
		cameraFollowing = !cameraFollowing;

        if (!cameraFollowing) {
            camera->Set(glm::vec3(0, 25, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
        } else {
			// set camera to follow the train
            Rail r = rails[train.currentRailIndex];
            glm::vec3 pos = glm::mix(r.startPosition, r.endPosition, train.progress);
            glm::vec3 dir = glm::normalize(r.endPosition - r.startPosition);

            glm::vec3 camera_pos = pos - dir * cameraOffset + glm::vec3(0, cameraOffsetY, 0);
            camera->Set(camera_pos, pos, glm::vec3(0, 1, 0));
        }
    }
}

void Tema2::OnKeyRelease(int key, int mods)
{
}


void Tema2::OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY)
{
    // Add mouse move event

    if (window->MouseHold(GLFW_MOUSE_BUTTON_RIGHT))
    {
        float sensivityOX = 0.001f;
        float sensivityOY = 0.001f;

        if (window->GetSpecialKeyState() == 0) {
            renderCameraTarget = false;
            // TODO(student): Rotate the camera in first-person mode around
            // OX and OY using `deltaX` and `deltaY`. Use the sensitivity
            // variables for setting up the rotation speed.
            float angleOX = -deltaY * sensivityOX;
            camera->RotateFirstPerson_OX(angleOX);

            float angleOY = -deltaX * sensivityOY;
            camera->RotateFirstPerson_OY(angleOY);

        }

        if (window->GetSpecialKeyState() & GLFW_MOD_CONTROL) {
            renderCameraTarget = true;
            // TODO(student): Rotate the camera in third-person mode around
            // OX and OY using `deltaX` and `deltaY`. Use the sensitivity
            // variables for setting up the rotation speed.

            float angleOX = -deltaY * sensivityOX;
            camera->RotateThirdPerson_OX(angleOX);

            float angleOY = -deltaX * sensivityOY;
            camera->RotateThirdPerson_OY(angleOY);

        }
    }
}

void Tema2::OnMouseBtnPress(int mouseX, int mouseY, int button, int mods)
{
    // Add mouse button press event
}

void Tema2::OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods)
{
    // Add mouse button release event
}


void Tema2::OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY)
{
}

void Tema2::OnWindowResize(int width, int height)
{
    if (textRenderer)
		delete textRenderer;
    textRenderer = new gfxc::TextRenderer(window->props.selfDir, width, height);
    textRenderer->Load("assets/fonts/Hack-Bold.ttf", 50);

    float aspectRatio = (float)width / (float)height;
    projectionMatrix = glm::perspective(RADIANS(fov), aspectRatio, 0.01f, 200.0f);

    float square = (float) min(width, height);
    miniViewportArea = ViewportArea(width - square / 2.5f, height - square / 2.5f, square / 2.5f, square / 2.5f);
}

void Tema2::CreatePlain() {
    vector<VertexFormat> vertices
    {
        VertexFormat(glm::vec3(-25, 0.01f, -25), glm::vec3(0.56f, 0.87f, 0.49f)),
        VertexFormat(glm::vec3(-25, 0.01f, 25), glm::vec3(0.56f, 0.87f, 0.49f)),
        VertexFormat(glm::vec3(25, 0.01f, -25), glm::vec3(0.56f, 0.87f, 0.49f)),
        VertexFormat(glm::vec3(25, 0.01f, 25), glm::vec3(0.56f, 0.87f, 0.49f))
    };

    vector<unsigned int> indices =
    {
        0, 1, 2,
        2, 1, 3
    };

    meshes["plain_terrain"] = new Mesh("generated plain terrain");
    meshes["plain_terrain"]->InitFromData(vertices, indices);
}

void Tema2::CreateRivers() {
    vector<VertexFormat> vertices
    {
        // top right river
        VertexFormat(glm::vec3(10, 0.02f, -25), glm::vec3(0.0f, 0.53f, 0.74f)),
        VertexFormat(glm::vec3(25, 0.02f, 5), glm::vec3(0.0f, 0.53f, 0.74f)),
        VertexFormat(glm::vec3(14, 0.02f, -25), glm::vec3(0.0f, 0.53f, 0.74f)),
        VertexFormat(glm::vec3(25, 0.02f, 1), glm::vec3(0.0f, 0.53f, 0.74f)),

        // across the map river
        VertexFormat(glm::vec3(-25, 0.02f, 8), glm::vec3(0.0f, 0.53f, 0.74f)),
        VertexFormat(glm::vec3(25, 0.02f, 3), glm::vec3(0.0f, 0.53f, 0.74f)),
        VertexFormat(glm::vec3(-25, 0.02f, 12), glm::vec3(0.0f, 0.53f, 0.74f)),
        VertexFormat(glm::vec3(25, 0.02f, 8), glm::vec3(0.0f, 0.53f, 0.74f))
    };
    vector<unsigned int> indices =
    {
        // top right river
        0, 1, 2,
        2, 1, 3,
        // across the map river
        4, 6, 5,
        5, 6, 7
    };
    meshes["river_terrain"] = new Mesh("generated river terrain");
    meshes["river_terrain"]->InitFromData(vertices, indices);
}

void Tema2::CreateMountains() {
    vector<VertexFormat> vertices
    {
        // top left mountain
        VertexFormat(glm::vec3(-14, 0.02f, 2), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(-10, 0.02f, 2), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(-14, 0.02f, -15), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(-10, 0.02f, -15), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(2, 0.02f, -15), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(2, 0.02f, -10), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(-14, 0.02f, -10), glm::vec3(0.55f, 0.43f, 0.24f)),

        // down left mountain
        VertexFormat(glm::vec3(-25, 0.02f, 16), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(2, 0.02f, 16), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(-25, 0.02f, 20), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(2, 0.02f, 20), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(-2, 0.02f, 16), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(-2, 0.02f, 25), glm::vec3(0.55f, 0.43f, 0.24f)),
        VertexFormat(glm::vec3(2, 0.02f, 25), glm::vec3(0.55f, 0.43f, 0.24f))
    };
    vector<unsigned int> indices =
    {
        // top left mountain
        0, 1, 2,
        2, 1, 3,
        2, 6, 5,
        2, 5, 4,
        // down left mountain
        7, 9, 8,
        9, 10, 8,
        11, 12, 13,
        11, 13, 8
    };
    meshes["mountain_terrain"] = new Mesh("generated mountain terrain");
    meshes["mountain_terrain"]->InitFromData(vertices, indices);
}

void Tema2::CreateParallelepiped(float length, float height, float width, glm::vec3 color, const std::string &name) {

	float halfL = length / 2;
	float halfW = width / 2;
    vector<VertexFormat> vertices
    {
        VertexFormat(glm::vec3(-halfL, 0, halfW), color),
        VertexFormat(glm::vec3(halfL, 0, halfW), color),
        VertexFormat(glm::vec3(-halfL, height, halfW), color),
        VertexFormat(glm::vec3(halfL, height, halfW), color),
        VertexFormat(glm::vec3(-halfL, 0, -halfW), color),
        VertexFormat(glm::vec3(halfL, 0, -halfW), color),
        VertexFormat(glm::vec3(-halfL, height, -halfW), color),
        VertexFormat(glm::vec3(halfL, height, -halfW), color)
	};
    vector<unsigned int> indices =
    {
        0, 1, 2,
        1, 3, 2,
        2, 3, 7,
        2, 7, 6,
        1, 7, 3,
        1, 5, 7,
        6, 7, 4,
        7, 5, 4,
        0, 4, 1,
        1, 4, 5,
        2, 6, 4,
        0, 2, 4
    };
    meshes[name] = new Mesh("generated " + name);
    meshes[name]->InitFromData(vertices, indices);
}

void Tema2::CreateCylinder(float length, float radius, glm::vec3 color, const std::string& name) {
    vector<VertexFormat> vertices;
    vector<unsigned int> indices;
    float angle = 0.0;

    while (angle < 2 * M_PI) {
        float y = radius * cos(angle);
        float z = radius * sin(angle);

        vertices.emplace_back(glm::vec3(0.0f, y, z), color);
        vertices.emplace_back(glm::vec3(length, y, z), color);
        angle += 0.1;
    }

    for (int i = 0; i < vertices.size() / 2 - 1; i++) {
        // first triangle
        indices.push_back(2 * i);
        indices.push_back(2 * i + 2);
        indices.push_back(2 * i + 1);

        // second triangle to complete the rectangle
        indices.push_back(2 * i + 2);
        indices.push_back(2 * i + 3);
        indices.push_back(2 * i + 1);
    }

    // closing the cylinder
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(vertices.size() - 2);
    indices.push_back(1);
    indices.push_back(vertices.size() - 1);
    indices.push_back(vertices.size() - 2);

    int offset = vertices.size();
    vertices.push_back(VertexFormat(glm::vec3(0.0f, 0.0f, 0.0f), color));
    vertices.push_back(VertexFormat(glm::vec3(length, 0.0f, 0.0f), color));

    for (int i = 0; i < offset - 2; i += 2) {
        indices.push_back(offset);
        indices.push_back(i);
        indices.push_back(i + 2);
    }
    indices.push_back(offset);
    indices.push_back(offset - 2);
    indices.push_back(0);

    for (int i = 1; i < offset - 2; i += 2) {
        indices.push_back(offset + 1);
        indices.push_back(i + 2);
        indices.push_back(i);
    }
    indices.push_back(offset + 1);
    indices.push_back(1);
    indices.push_back(offset - 1);

    meshes[name] = new Mesh("generated " + name);
    meshes[name]->InitFromData(vertices, indices);
}

void Tema2::CreateCone(float height, float radius, glm::vec3 color, const std::string& name) {
    vector<VertexFormat> vertices;
    vector<unsigned int> indices;
    float angle = 0.0;

    while (angle < 2 * M_PI) {
        float x = radius * cos(angle);
        float z = radius * sin(angle);

        vertices.emplace_back(glm::vec3(x, 0.0f, z), color);
        angle += 0.1;
    }

    vertices.push_back(VertexFormat(glm::vec3(0.0f, height, 0.0f), color));
    int peak = vertices.size() - 1;

    for (int i = 0; i < peak - 1; i++) {
        indices.push_back(peak);
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    // closing the cone
    indices.push_back(peak);
    indices.push_back(peak - 1);
    indices.push_back(0);

	vertices.push_back(VertexFormat(glm::vec3(0.0f, 0.0f, 0.0f), color));
	int baseCenter = vertices.size() - 1;

    for (int i = 0; i < peak - 1; i++) {
        indices.push_back(baseCenter);
        indices.push_back(i + 1);
        indices.push_back(i);
	}
	indices.push_back(baseCenter);
	indices.push_back(0);
	indices.push_back(peak - 1);

    meshes[name] = new Mesh("generated " + name);
    meshes[name]->InitFromData(vertices, indices);
}

void Tema2::CreatePyramid(float base, float height, glm::vec3 color, const std::string& name) {
    vector<VertexFormat> vertices
    {
        VertexFormat(glm::vec3(0, 0, 0), color),
        VertexFormat(glm::vec3(base, 0, 0), color),
        VertexFormat(glm::vec3(base, 0, base), color),
        VertexFormat(glm::vec3(0, 0, base), color),
        VertexFormat(glm::vec3(base / 2, height, base / 2), color)
    };
    vector<unsigned int> indices =
    {
        0, 1, 2,
        0, 2, 3,
        4, 0, 1,
        4, 1, 2,
        4, 2, 3,
        4, 3, 0
    };
    meshes[name] = new Mesh("generated " + name);
    meshes[name]->InitFromData(vertices, indices);
}

void Tema2::CreateRailway() {
    rails.push_back(Rail(glm::vec3(-16, 0.03f, 21), glm::vec3(-16, 0.03f, 24), 0)); // 0
    rails.push_back(Rail(glm::vec3(-16, 0.03f, 24), glm::vec3(-22, 0.03f, 24), 0)); // 1
    rails.push_back(Rail(glm::vec3(-22, 0.03f, 24), glm::vec3(-22, 0.03f, 21), 0)); // 2
    rails.push_back(Rail(glm::vec3(-22, 0.03f, 21), glm::vec3(-16, 0.03f, 21), 0)); // 3
    rails.push_back(Rail(glm::vec3(-16, 0.03f, 21), glm::vec3(-15, 0.03f, 20), 0)); // 4
	rails.push_back(Rail(glm::vec3(-15, 0.03f, 20), glm::vec3(-13, 0.03f, 18), 1)); // 5
	rails.push_back(Rail(glm::vec3(-13, 0.03f, 18), glm::vec3(2, 0.03f, 18), 1)); // 6
    rails.push_back(Rail(glm::vec3(2, 0.03f, 18), glm::vec3(4, 0.03f, 22), 0)); // 7
    rails.push_back(Rail(glm::vec3(4, 0.03f, 22), glm::vec3(12, 0.03f, 16), 0)); // 8
	rails.push_back(Rail(glm::vec3(12, 0.03f, 16), glm::vec3(5, 0.03f, 15), 0)); // 9
    rails.push_back(Rail(glm::vec3(5, 0.03f, 15), glm::vec3(4, 0.03f, 22), 0)); // 10
    rails.push_back(Rail(glm::vec3(5, 0.03f, 15), glm::vec3(2, 0.03f, 18), 0)); // 11
    rails.push_back(Rail(glm::vec3(12, 0.03f, 16), glm::vec3(18, 0.03f, 18), 0)); // 12
    rails.push_back(Rail(glm::vec3(18, 0.03f, 18), glm::vec3(20, 0.03f, 20), 0)); // 13
    rails.push_back(Rail(glm::vec3(20, 0.03f, 20), glm::vec3(20, 0.03f, 23), 0)); // 14
    rails.push_back(Rail(glm::vec3(20, 0.03f, 23), glm::vec3(23, 0.03f, 23), 0)); // 15
    rails.push_back(Rail(glm::vec3(23, 0.03f, 23), glm::vec3(23, 0.03f, 20), 0)); // 16
    rails.push_back(Rail(glm::vec3(23, 0.03f, 20), glm::vec3(20, 0.03f, 20), 0)); // 17
    rails.push_back(Rail(glm::vec3(5, 0.03f, 15), glm::vec3(1, 0.03f, 10), 0)); // 18
    rails.push_back(Rail(glm::vec3(1, 0.03f, 10), glm::vec3(0, 0.03f, 5), 2)); // 19
	rails.push_back(Rail(glm::vec3(0, 0.03f, 5), glm::vec3(-2, 0.03f, 1), 0)); // 20
	rails.push_back(Rail(glm::vec3(-2, 0.03f, 1), glm::vec3(2, 0.03f, 1), 0)); // 21
    rails.push_back(Rail(glm::vec3(2, 0.03f, 1), glm::vec3(2, 0.03f, -2), 0)); // 22
    rails.push_back(Rail(glm::vec3(2, 0.03f, -2), glm::vec3(0, 0.03f, -3), 0)); // 23
    rails.push_back(Rail(glm::vec3(0, 0.03f, -3), glm::vec3(-2, 0.03f, -2), 0)); // 24
    rails.push_back(Rail(glm::vec3(-2, 0.03f, -2), glm::vec3(-2, 0.03f, 1), 0)); // 25
	rails.push_back(Rail(glm::vec3(-2, 0.03f, 1), glm::vec3(-13, 0.03f, 7), 0)); // 26
    rails.push_back(Rail(glm::vec3(-13, 0.03f, 7), glm::vec3(-12, 0.03f, 2), 0)); // 27
    rails.push_back(Rail(glm::vec3(-13, 0.03f, 7), glm::vec3(-12, 0.03f, 11), 2)); // 28
    rails.push_back(Rail(glm::vec3(-12, 0.03f, 11), glm::vec3(-13, 0.03f, 16), 0)); // 29
    rails.push_back(Rail(glm::vec3(-13, 0.03f, 16), glm::vec3(-13, 0.03f, 18), 1)); // 30
    rails.push_back(Rail(glm::vec3(-12, 0.03f, 2), glm::vec3(-12, 0.03f, -12), 1)); // 31
    rails.push_back(Rail(glm::vec3(-12, 0.03f, -12), glm::vec3(-14, 0.03f, -15), 1)); // 32
    rails.push_back(Rail(glm::vec3(-14, 0.03f, -15), glm::vec3(-18, 0.03f, -19), 0)); // 33
    rails.push_back(Rail(glm::vec3(-18, 0.03f, -19), glm::vec3(-21, 0.03f, -21), 0)); // 34
    rails.push_back(Rail(glm::vec3(-21, 0.03f, -21), glm::vec3(-23, 0.03f, -21), 0)); // 35
    rails.push_back(Rail(glm::vec3(-23, 0.03f, -21), glm::vec3(-23, 0.03f, -23), 0)); // 36
    rails.push_back(Rail(glm::vec3(-23, 0.03f, -23), glm::vec3(-21, 0.03f, -23), 0)); // 37
    rails.push_back(Rail(glm::vec3(-21, 0.03f, -23), glm::vec3(-21, 0.03f, -21), 0)); // 38
    rails.push_back(Rail(glm::vec3(-18, 0.03f, -19), glm::vec3(-22, 0.03f, -4), 0)); // 39
    rails.push_back(Rail(glm::vec3(-22, 0.03f, -4), glm::vec3(-13, 0.03f, 7), 0)); // 40
    rails.push_back(Rail(glm::vec3(-12, 0.03f, -12), glm::vec3(-10, 0.03f, -10), 1)); // 41
    rails.push_back(Rail(glm::vec3(-10, 0.03f, -10), glm::vec3(-5, 0.03f, -5), 0)); // 42
    rails.push_back(Rail(glm::vec3(-5, 0.03f, -5), glm::vec3(5, 0.03f, -8), 0)); // 43
    rails.push_back(Rail(glm::vec3(-5, 0.03f, -5), glm::vec3(-2, 0.03f, -2), 0)); // 44
    rails.push_back(Rail(glm::vec3(5, 0.03f, -8), glm::vec3(0, 0.03f, -3), 0)); // 45
    rails.push_back(Rail(glm::vec3(-12, 0.03f, -12), glm::vec3(2, 0.03f, -12), 1)); // 46
    rails.push_back(Rail(glm::vec3(2, 0.03f, -12), glm::vec3(11, 0.03f, -14), 0)); // 47
    rails.push_back(Rail(glm::vec3(11, 0.03f, -14), glm::vec3(5, 0.03f, -8), 0)); // 48
    rails.push_back(Rail(glm::vec3(11, 0.03f, -14), glm::vec3(-1, 0.03f, -20), 0)); // 49
    rails.push_back(Rail(glm::vec3(-1, 0.03f, -20), glm::vec3(-18, 0.03f, -19), 0)); // 50
    rails.push_back(Rail(glm::vec3(11, 0.03f, -14), glm::vec3(15, 0.03f, -15), 0)); // 51
	rails.push_back(Rail(glm::vec3(15, 0.03f, -15), glm::vec3(18, 0.03f, -17), 2)); // 52
    rails.push_back(Rail(glm::vec3(18, 0.03f, -17), glm::vec3(20, 0.03f, -20), 0)); // 53
    rails.push_back(Rail(glm::vec3(20, 0.03f, -20), glm::vec3(20, 0.03f, -23), 0)); // 54
    rails.push_back(Rail(glm::vec3(20, 0.03f, -23), glm::vec3(23, 0.03f, -23), 0)); // 55
    rails.push_back(Rail(glm::vec3(23, 0.03f, -23), glm::vec3(23, 0.03f, -20), 0)); // 56
    rails.push_back(Rail(glm::vec3(23, 0.03f, -20), glm::vec3(20, 0.03f, -20), 0)); // 57
    rails.push_back(Rail(glm::vec3(11, 0.03f, -14), glm::vec3(15, 0.03f, -5), 0)); // 58
    rails.push_back(Rail(glm::vec3(15, 0.03f, -5), glm::vec3(10, 0.03f, -3), 0)); // 59
    rails.push_back(Rail(glm::vec3(10, 0.03f, -3), glm::vec3(2, 0.03f, -2), 0)); // 60
    rails.push_back(Rail(glm::vec3(10, 0.03f, -3), glm::vec3(11, 0.03f, -14), 0)); // 61
    rails.push_back(Rail(glm::vec3(15, 0.03f, -5), glm::vec3(13, 0.03f, 4), 0)); // 62
    rails.push_back(Rail(glm::vec3(13, 0.03f, 4), glm::vec3(10, 0.03f, -3), 0)); // 63
    rails.push_back(Rail(glm::vec3(13, 0.03f, 4), glm::vec3(2, 0.03f, 1), 0)); // 64
    rails.push_back(Rail(glm::vec3(13, 0.03f, 4), glm::vec3(12, 0.03f, 9), 2)); // 65
    rails.push_back(Rail(glm::vec3(12, 0.03f, 9), glm::vec3(12, 0.03f, 16), 0)); // 66
}

void Tema2::DrawRails() {
    for (int i = 0 ; i < rails.size(); i++) {
        glm::vec3 start = rails[i].startPosition;
        glm::vec3 end = rails[i].endPosition;
		float length = glm::distance(start, end);

		// choose color based on rail type
        if (rails[i].type_rail == 0)
			CreateParallelepiped(length, 0.1f, 0.5f, rail_color, "rail_" + to_string(i));
		else if (rails[i].type_rail == 1)
            CreateParallelepiped(length, 0.1f, 0.5f, tunnel_color, "rail_" + to_string(i));
        else if (rails[i].type_rail == 2)
            CreateParallelepiped(length, 0.1f, 0.5f, bridge_color, "rail_" + to_string(i));
	}
}

void Tema2::CreateTrain() {
	CreateParallelepiped(1, 0.05f, 0.3f, glm::vec3(1, 1, 0), "train_base");
	CreateParallelepiped(0.4f, 0.3f, 0.3f, glm::vec3(0, 1, 0), "train_cabin");
	CreateCylinder(0.5f, 0.075f, glm::vec3(0, 0, 1), "train_big_engine");
	CreateCylinder(0.01f, 0.01f, glm::vec3(1, 0.5f, 1), "train_small_engine");
	CreateCylinder(0.1f, 0.05f, glm::vec3(1, 0, 0), "train_wheel");
}

void Tema2::CreateWagon() {
    CreateParallelepiped(1, 0.05f, 0.3f, glm::vec3(1, 1, 0), "wagon_base");
    CreateParallelepiped(1, 0.3f, 0.3f, glm::vec3(0, 1, 0), "wagon_cabin");
}

void Tema2::DrawTrain(float deltaTimeSeconds) {
    Rail r = rails[train.currentRailIndex];
	float old_progress = train.progress;

	// move train on the rail
    if (train.progress < 1.0f) {
        train.progress += (deltaTimeSeconds * train.speed) / glm::distance(r.startPosition, r.endPosition);
    }

	gap = 0.0f;

    if (train.progress >= 1.0f) {
        int next = -1;

        if (rails[train.currentRailIndex].branches_end.size() > 1) {
            train.progress = 1.0f;
            gap = 1.0 - old_progress;
        } else {
			// switch to the next rail - continue straight
            train.progress = (train.progress - 1.0f) * glm::distance(r.startPosition, r.endPosition);
			next = rails[train.currentRailIndex].branches_end[0];
			train.progress /= glm::distance(rails[next].startPosition, rails[next].endPosition);
			// switch rail direction if needed
            if (rails[train.currentRailIndex].endPosition != rails[next].startPosition) {
                glm::vec3 aux = rails[next].startPosition;
                rails[next].startPosition = rails[next].endPosition;
                rails[next].endPosition = aux;

                vector<int> temp = rails[next].branches_end;
                rails[next].branches_end = rails[next].branches_start;
                rails[next].branches_start = temp;
            }
            train.currentRailIndex = next;
            wagon.railQueue.push(next);
        }
    }

    r = rails[train.currentRailIndex];
    glm::vec3 pos = glm::mix(r.startPosition, r.endPosition, train.progress);
    float angle = -atan2(r.endPosition.z - r.startPosition.z, r.endPosition.x - r.startPosition.x);

    // create locomotive
    glm::mat4 model = glm::mat4(1);
    model = glm::translate(model, pos + glm::vec3(0, 0.2f, 0));
    glm::mat4 baseModel = glm::rotate(model, angle, glm::vec3(0, 1, 0));
    RenderMesh(meshes["train_base"], shaders["VertexColor"], baseModel);

    model = glm::translate(baseModel, glm::vec3(-0.3f, 0.05f, 0));
    RenderMesh(meshes["train_cabin"], shaders["VertexColor"], model);

    model = glm::translate(baseModel, glm::vec3(-0.1f, 0.125f, 0));
    RenderMesh(meshes["train_big_engine"], shaders["VertexColor"], model);

    model = glm::translate(baseModel, glm::vec3(0.4f, 0.125f, 0));
    RenderMesh(meshes["train_small_engine"], shaders["VertexColor"], model);

    for (int i = -3; i < 4; i++) {
        model = glm::translate(baseModel, glm::vec3(0, -0.05f, 0));
        model = glm::rotate(model, RADIANS(90.0f), glm::vec3(0, 1, 0));
        model = glm::translate(model, glm::vec3(0.05f, 0, 0.12f * i));
        RenderMesh(meshes["train_wheel"], shaders["VertexColor"], model);

        model = glm::translate(model, glm::vec3(-0.2f, 0, 0));
        RenderMesh(meshes["train_wheel"], shaders["VertexColor"], model);
    }
}

void Tema2::DrawWagon(float deltaTimeSeconds) {
    if (wagon.railQueue.size() == 0)
        return;
    Rail r = rails[wagon.railQueue.front()];

	// move wagon on the rail
	if (train.progress < 1.0f)
        wagon.progress += (deltaTimeSeconds * wagon.speed) / glm::distance(r.startPosition, r.endPosition);
    else
        wagon.progress += gap;

	// switch to the next rail if needed
    if (wagon.progress >= 1.0f && wagon.railQueue.front() != train.currentRailIndex) {
        wagon.progress = (wagon.progress - 1.0f) * glm::distance(r.startPosition, r.endPosition);
        wagon.railQueue.pop();
		wagon.progress /= glm::distance(rails[wagon.railQueue.front()].startPosition, rails[wagon.railQueue.front()].endPosition);
    }

    r = rails[wagon.railQueue.front()];
    glm::vec3 pos = glm::mix(r.startPosition, r.endPosition, wagon.progress);
    float angle = -atan2(r.endPosition.z - r.startPosition.z, r.endPosition.x - r.startPosition.x);

    glm::mat4 model = glm::mat4(1);
    model = glm::translate(model, pos + glm::vec3(0, 0.2f, 0));
    glm::mat4 baseModel = glm::rotate(model, angle, glm::vec3(0, 1, 0));
    RenderMesh(meshes["wagon_base"], shaders["VertexColor"], baseModel);

    model = glm::translate(baseModel, glm::vec3(0, 0.05f, 0));
    RenderMesh(meshes["wagon_cabin"], shaders["VertexColor"], model);

    for (int i = -1; i <= 1; i += 2) {
        model = glm::translate(baseModel, glm::vec3(0, -0.05f, 0));
        model = glm::rotate(model, RADIANS(90.0f), glm::vec3(0, 1, 0));
        model = glm::translate(model, glm::vec3(0.05f, 0, 0.4f * i));
        RenderMesh(meshes["train_wheel"], shaders["VertexColor"], model);

        model = glm::translate(model, glm::vec3(-0.2f, 0, 0));
        RenderMesh(meshes["train_wheel"], shaders["VertexColor"], model);
    }
}

void Tema2::AddBranches() {

    // railway 0
    rails[0].branches_end = { 1 };
    rails[0].branches_start = { 3, 4 };

    // railway 1
    rails[1].branches_end = { 2 };
    rails[1].branches_start = { 0 };

    // railway 2
    rails[2].branches_end = { 3 };
    rails[2].branches_start = { 1 };

    // railway 3
    rails[3].branches_end = { 4, 0 };
    rails[3].branches_start = { 2 };

    // railway 4
    rails[4].branches_end = { 5 };
    rails[4].branches_start = { 0, 3 };

    // railway 5
    rails[5].branches_end = { 30, 6 };
    rails[5].branches_start = { 4 };

    // railway 6
    rails[6].branches_end = { 11, 7 };
    rails[6].branches_start = { 5, 30 };

    // railway 7
    rails[7].branches_end = { 10, 8 };
    rails[7].branches_start = { 6, 11 };

    // railway 8
    rails[8].branches_end = { 9, 66, 12 };
    rails[8].branches_start = { 7, 10 };

    // railway 9
    rails[9].branches_end = { 10, 11, 18 };
    rails[9].branches_start = { 66, 12, 8 };

    // railway 10
    rails[10].branches_end = { 8, 7 };
    rails[10].branches_start = { 11, 18, 9 };

    // railway 11
    rails[11].branches_end = { 7, 6 };
    rails[11].branches_start = { 18, 9, 10 };

    // railway 12
    rails[12].branches_end = { 13 };
    rails[12].branches_start = { 8, 9, 66 };

    // railway 13
    rails[13].branches_end = { 17, 14 };
    rails[13].branches_start = { 12 };

    // railway 14
    rails[14].branches_end = { 15 };
    rails[14].branches_start = { 13, 17 };

    // railway 15
    rails[15].branches_end = { 16 };
    rails[15].branches_start = { 14 };

    // railway 16
    rails[16].branches_end = { 17 };
    rails[16].branches_start = { 15 };

    // railway 17
    rails[17].branches_end = { 14, 13 };
    rails[17].branches_start = { 16 };

    // railway 18
    rails[18].branches_end = { 19 };
    rails[18].branches_start = { 9, 10, 11 };

    // railway 19
    rails[19].branches_end = { 20 };
    rails[19].branches_start = { 18 };

    // railway 20
    rails[20].branches_end = { 26, 25, 21 };
    rails[20].branches_start = { 19 };

    // railway 21
    rails[21].branches_end = { 22, 64 };
    rails[21].branches_start = { 20, 26, 25 };

    // railway 22
    rails[22].branches_end = { 23, 60 };
    rails[22].branches_start = { 64, 21 };

    // railway 23
    rails[23].branches_end = { 24, 45 };
    rails[23].branches_start = { 60, 22 };

    // railway 24
    rails[24].branches_end = { 25, 44 };
    rails[24].branches_start = { 45, 23 };

    // railway 25
    rails[25].branches_end = { 21, 20, 26 };
    rails[25].branches_start = { 44, 24 };

    // railway 26
    rails[26].branches_end = { 28, 40, 27 };
    rails[26].branches_start = { 25, 21, 20 };

    // railway 27
    rails[27].branches_end = { 31 };
    rails[27].branches_start = { 26, 28, 40 };

    // railway 28
    rails[28].branches_end = { 29 };
    rails[28].branches_start = { 40, 27, 26 };

    // railway 29
    rails[29].branches_end = { 30 };
    rails[29].branches_start = { 28 };

    // railway 30
    rails[30].branches_end = { 6, 5 };
    rails[30].branches_start = { 29 };

    // railway 31
    rails[31].branches_end = { 32, 46, 41 };
    rails[31].branches_start = { 27 };

    // railway 32
    rails[32].branches_end = { 33 };
    rails[32].branches_start = { 46, 41, 31 };

    // railway 33
    rails[33].branches_end = { 39, 34, 50 };
    rails[33].branches_start = { 32 };

    // railway 34
    rails[34].branches_end = { 35, 38 };
    rails[34].branches_start = { 50, 33, 39 };

    // railway 35
    rails[35].branches_end = { 36 };
    rails[35].branches_start = { 38, 34 };

    // railway 36
    rails[36].branches_end = { 37 };
    rails[36].branches_start = { 35 };

    // railway 37
    rails[37].branches_end = { 38 };
    rails[37].branches_start = { 36 };

    // railway 38
    rails[38].branches_end = { 34, 35 };
    rails[38].branches_start = { 37 };

    // railway 39
    rails[39].branches_end = { 40 };
    rails[39].branches_start = { 34, 50, 33 };

    // railway 40
    rails[40].branches_end = { 27, 26, 28 };
    rails[40].branches_start = { 39 };

    // railway 41
    rails[41].branches_end = { 42 };
    rails[41].branches_start = { 31, 32, 46 };

    // railway 42
    rails[42].branches_end = { 43, 44 };
    rails[42].branches_start = { 41 };

    // railway 43
    rails[43].branches_end = { 48, 45 };
    rails[43].branches_start = { 44, 42 };

    // railway 44
    rails[44].branches_end = { 24, 25 };
    rails[44].branches_start = { 42, 43 };

    // railway 45
    rails[45].branches_end = { 23, 24 };
    rails[45].branches_start = { 43, 48 };

    // railway 46
    rails[46].branches_end = { 47 };
    rails[46].branches_start = { 41, 31, 32 };

    // railway 47
    rails[47].branches_end = { 49, 51, 58, 61, 48 };
    rails[47].branches_start = { 46 };

    // railway 48
    rails[48].branches_end = { 45, 43 };
    rails[48].branches_start = { 47, 49, 51, 58, 61 };

    // railway 49
    rails[49].branches_end = { 50 };
    rails[49].branches_start = { 51, 58, 61, 48, 47 };

    // railway 50
    rails[50].branches_end = { 33, 39, 34 };
    rails[50].branches_start = { 49 };

    // railway 51
    rails[51].branches_end = { 52 };
    rails[51].branches_start = { 58, 61, 48, 47, 49 };

    // railway 52
    rails[52].branches_end = { 53 };
    rails[52].branches_start = { 51 };

    // railway 53
    rails[53].branches_end = { 54, 57 };
    rails[53].branches_start = { 52 };

    // railway 54
    rails[54].branches_end = { 55 };
    rails[54].branches_start = { 57, 53 };

    // railway 55
    rails[55].branches_end = { 56 };
    rails[55].branches_start = { 54 };

    // railway 56
    rails[56].branches_end = { 57 };
    rails[56].branches_start = { 55 };

    // railway 57
    rails[57].branches_end = { 53, 54 };
    rails[57].branches_start = { 56 };

    // railway 58
    rails[58].branches_end = { 62, 59 };
    rails[58].branches_start = { 61, 48, 47, 49, 51 };

    // railway 59
    rails[59].branches_end = { 63, 60, 61 };
    rails[59].branches_start = { 58, 62 };

    // railway 60
    rails[60].branches_end = { 22, 23 };
    rails[60].branches_start = { 61, 59, 63 };

    // railway 61
    rails[61].branches_end = { 48, 47, 49, 51, 58 };
    rails[61].branches_start = { 59, 63, 60 };

    // railway 62
    rails[62].branches_end = { 65, 64, 63 };
    rails[62].branches_start = { 59, 58 };

    // railway 63
    rails[63].branches_end = { 60, 61, 59 };
    rails[63].branches_start = { 62, 65, 64 };

    // railway 64
    rails[64].branches_end = { 21, 22 };
    rails[64].branches_start = { 63, 62, 65 };

    // railway 65
    rails[65].branches_end = { 66 };
    rails[65].branches_start = { 64, 63, 62 };

    // railway 66
    rails[66].branches_end = { 12, 8, 9 };
    rails[66].branches_start = { 65 };

}

void Tema2::CreateArrow() {
    vector<VertexFormat> vertices
    {
        VertexFormat(glm::vec3(-0.2f, 0, -0.1f), glm::vec3(1, 0, 0)),
        VertexFormat(glm::vec3(0, 0, -0.1f), glm::vec3(1, 0, 0)),
        VertexFormat(glm::vec3(0, 0, 0.1f), glm::vec3(1, 0, 0)),
        VertexFormat(glm::vec3(-0.2f, 0, 0.1f), glm::vec3(1, 0, 0)),
        VertexFormat(glm::vec3(0, 0, 0.2f), glm::vec3(1, 0, 0)),
        VertexFormat(glm::vec3(0, 0, -0.2f), glm::vec3(1, 0, 0)),
        VertexFormat(glm::vec3(0.2f, 0, 0), glm::vec3(1, 0, 0))
        };
    vector<unsigned int> indices =
    {
        0, 2, 1,
        0, 3, 2,
        4, 6, 5,
    };
    meshes["arrow"] = new Mesh("generated arrow");
    meshes["arrow"]->InitFromData(vertices, indices);
}

void Tema2::DrawArrow() {
    glm::mat4 model = glm::mat4(1);
    Rail r = rails[rails[train.currentRailIndex].branches_end.at(junctionIndex)];
    glm::vec3 start = r.startPosition;
    glm::vec3 end = r.endPosition;

    if (r.startPosition != rails[train.currentRailIndex].endPosition) {
        start = r.endPosition;
        end = r.startPosition;
    }

    model = glm::translate(model, start + glm::normalize(end - start) + glm::vec3(0, 0.5f, 0));
    model = glm::rotate(model, -atan2(end.z - start.z, end.x - start.x), glm::vec3(0, 1, 0));
    RenderMesh(meshes["arrow"], shaders["VertexColor"], model);
}

void Tema2::DrawEndGame() {
    glm::vec2 resolution = window->GetResolution();

    textRenderer->RenderText("GAME OVER!", (resolution.x / 2 - 5 * 50 * 0.6f) , 50, 1.0f, glm::vec3(1, 1, 1));
    textRenderer->RenderText("Missions Completed: " + std::to_string(missions), (resolution.x / 2 - 10.5f * 50 * 0.6f), 100, 1.0f, glm::vec3(1, 1, 1));
    textRenderer->RenderText("Press R to replay!", (resolution.x / 2 - 9 * 50 * 0.6f), resolution.y - 100, 1.0f, glm::vec3(1, 1, 1));
}

void Tema2::PickUpLogic() {
	Rail r = rails[train.currentRailIndex];
	glm::vec3 train_pos = glm::mix(r.startPosition, r.endPosition, train.progress);
    train_pos.y = 0;
	float pickup_distance = 0.05f;

    if (station_1_items > 0 && timer1 >= respawn_time && glm::distance(train_pos, item1_pos) < pickup_distance) {
        station_1_items--;
        timer1 = 0.0f;
		cout << "picked up item from station 1. Remaining items: " << station_1_items << endl;
	}

    if (station_2_items > 0 && timer2 >= respawn_time && glm::distance(train_pos, item2_pos) < pickup_distance) {
        station_2_items--;
        timer2 = 0.0f;
		cout << "picked up item from station 2. Remaining items: " << station_2_items << endl;
    }

    if (station_3_items > 0 && timer3 >= respawn_time && glm::distance(train_pos, item3_pos) < pickup_distance) {
        station_3_items--;
        timer3 = 0.0f;
		cout << "picked up item from station 3. Remaining items: " << station_3_items << endl;
    }

    if (station_4_items > 0 && timer4 >= respawn_time && glm::distance(train_pos, item4_pos) < pickup_distance) {
        station_4_items--;
        timer4 = 0.0f;
		cout << "picked up item from station 4. Remaining items: " << station_4_items << endl;
    }
}

void Tema2::RandomizeObjects() {
    int initial_items = 5;

    while (initial_items > 0) {
        int pos = rand() % 4 + 1;

        if (pos == 1 && station_1_items < 2) {
            initial_items--;
            station_1_items++;
        }
        if (pos == 2 && station_2_items < 2) {
            initial_items--;
            station_2_items++;
        }
        if (pos == 3 && station_3_items < 2) {
            initial_items--;
            station_3_items++;
        }
        if (pos == 4 && station_4_items < 2) {
            initial_items--;
            station_4_items++;
        }
    }
}

void Tema2::DoneMission() {
    if (station_1_items == 0 && station_2_items == 0 && station_3_items == 0 && station_4_items == 0) {
        glm::mat4 model = glm::mat4(1);

        model = glm::translate(model, glm::vec3(0, 0.21f, 1));
        model = glm::scale(model, glm::vec3(0.2));
        RenderMesh(meshes["central_station"], shaders["central_station"], model);

        Rail r = rails[train.currentRailIndex];
        glm::vec3 train_pos = glm::mix(r.startPosition, r.endPosition, train.progress);

		// check if train is near the central station
        train_pos.y = 0;
        float pickup_distance = 0.05f;

        if (glm::distance(train_pos, done_mission) < pickup_distance) {
            missions++;
            timer = 0.0f;
            timer1 = respawn_time;
            timer2 = respawn_time;
            timer3 = respawn_time;
            timer4 = respawn_time;
            RandomizeObjects();
            cout << "done_mission: " << missions  << endl;
        }
    }
}

void Tema2::DrawObjects() {
    int count = station_1_items + station_2_items + station_3_items + station_4_items;
    float offset = 5.0f / count;
	glm::mat4 model = glm::mat4(1);
    float current_offset = -2.5f + offset / 2;

    for (int i = 0; i < station_1_items; i++) {
        model = glm::mat4(1);
        model = glm::translate(model, glm::vec3(current_offset, 1.5f, -0.5f));
        model = glm::scale(model, glm::vec3(0.3f));
        RenderMesh(meshes["station_1"], shaders["VertexColor"], model);
        current_offset += offset;
    }

    for (int i = 0; i < station_2_items; i++) {
        model = glm::mat4(1);
        model = glm::translate(model, glm::vec3(current_offset, 1.65f, -0.5f));
        model = glm::scale(model, glm::vec3(0.3f));
        RenderMesh(meshes["station_2"], shaders["VertexColor"], model);
        current_offset += offset;
    }

    for (int i = 0; i < station_3_items; i++) {
        model = glm::mat4(1);
        model = glm::translate(model, glm::vec3(current_offset, 1.5f, -0.5f));
        model = glm::scale(model, glm::vec3(0.3f));
        RenderMesh(meshes["station_3"], shaders["VertexColor"], model);
        current_offset += offset;
    }

    for (int i = 0; i < station_4_items; i++) {
        model = glm::mat4(1);
        model = glm::translate(model, glm::vec3(current_offset, 1.5f, -0.75f));
        model = glm::scale(model, glm::vec3(0.3f));
        RenderMesh(meshes["station_4"], shaders["VertexColor"], model);
        current_offset += offset;
	}  
}

void Tema2::DrawAllScene(float deltaTimeSeconds) {
    RenderMesh(meshes["plain_terrain"], shaders["VertexColor"], glm::mat4(1));
    RenderMesh(meshes["river_terrain"], shaders["VertexColor"], glm::mat4(1));
    RenderMesh(meshes["mountain_terrain"], shaders["VertexColor"], glm::mat4(1));

    glm::mat4 model = glm::mat4(1);
    model = glm::translate(model, glm::vec3(0, 0.01f, -0.5f));
    RenderMesh(meshes["central_station"], shaders["central_station"], model);

    model = glm::mat4(1);
    model = glm::translate(model, glm::vec3(-19, 0.01f, 22.5f));
    RenderMesh(meshes["station_1"], shaders["VertexColor"], model);

    // object for station 1
    if (station_1_items > 0 && timer1 >= respawn_time) {
        model = glm::translate(model, glm::vec3(0, 0.2f, 1.5f));
        model = glm::scale(model, glm::vec3(0.2f));
        RenderMesh(meshes["station_1"], shaders["VertexColor"], model);
    }

    model = glm::mat4(1);
    model = glm::translate(model, glm::vec3(21, 0.51f, -21));
    RenderMesh(meshes["station_2"], shaders["VertexColor"], model);

    // object for station 2
    if (station_2_items > 0 && timer2 >= respawn_time) {
        model = glm::translate(model, glm::vec3(0, 0, -2));
        model = glm::scale(model, glm::vec3(0.2f));
        RenderMesh(meshes["station_2"], shaders["VertexColor"], model);
    }

    model = glm::mat4(1);
    model = glm::translate(model, glm::vec3(-22, 0.01f, -22));
    RenderMesh(meshes["station_3"], shaders["VertexColor"], model);

    // object for station 3
    if (station_3_items > 0 && timer3 >= respawn_time) {
        model = glm::translate(model, glm::vec3(0, 0.2f, -1));
        model = glm::scale(model, glm::vec3(0.2f));
        RenderMesh(meshes["station_3"], shaders["VertexColor"], model);
    }

    model = glm::mat4(1);
    model = glm::translate(model, glm::vec3(21, 0.01f, 21));
    RenderMesh(meshes["station_4"], shaders["VertexColor"], model);

	// object for station 4
    if (station_4_items > 0 && timer4 >= respawn_time) {
        model = glm::translate(model, glm::vec3(0.4f, 0.2f, 2));
        model = glm::scale(model, glm::vec3(0.2f));
        RenderMesh(meshes["station_4"], shaders["VertexColor"], model);
    }

	// rails rendering
    for (int i = 0; i < rails.size(); i++) {
        model = glm::mat4(1);
        model = glm::translate(model, (rails[i].startPosition + rails[i].endPosition) / 2.0f);
        model = glm::rotate(model, -atan2(rails[i].endPosition.z - rails[i].startPosition.z, rails[i].endPosition.x - rails[i].startPosition.x), glm::vec3(0, 1, 0));

        RenderMesh(meshes["rail_" + to_string(i)], shaders["VertexColor"], model);
    }

    DrawTrain(deltaTimeSeconds);
    DrawWagon(deltaTimeSeconds);
    DrawObjects();

    if (train.progress >= 1.0f) {
        DrawArrow();
    }
}