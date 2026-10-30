#pragma once

#include "components/simple_scene.h"
#include "lab_m1/tema2/lab_camera.h"
#include "components/text_renderer.h"
#include <queue>

namespace m1
{
    class Tema2 : public gfxc::SimpleScene
    {
     public:
        Tema2();
        ~Tema2();

        void Init() override;

     private:
        void FrameStart() override;
        void Update(float deltaTimeSeconds) override;
        void FrameEnd() override;

        void RenderMesh(Mesh *mesh, Shader *shader, const glm::mat4 &modelMatrix) override;

        void OnInputUpdate(float deltaTime, int mods) override;
        void OnKeyPress(int key, int mods) override;
        void OnKeyRelease(int key, int mods) override;
        void OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY) override;
        void OnMouseBtnPress(int mouseX, int mouseY, int button, int mods) override;
        void OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods) override;
        void OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY) override;
        void OnWindowResize(int width, int height) override;

        void CreatePlain();
		void CreateRivers();
		void CreateMountains();
		void CreateParallelepiped(float length, float height, float width, glm::vec3 color,  const std::string& name);
		void CreateCylinder(float radius, float height, glm::vec3 color, const std::string& name);
		void CreateCone(float height, float radius, glm::vec3 color, const std::string& name);
		void CreatePyramid(float base, float height, glm::vec3 color, const std::string& name);
        void CreateArrow();
        void CreateRailway();
        void DrawRails();
        void AddBranches();
        void CreateTrain();
        void CreateWagon();
		void DrawTrain(float deltaTimeSeconds);
		void DrawWagon(float deltaTimeSeconds);
        void DrawArrow();
        void DrawEndGame();
        void PickUpLogic();
        void DoneMission();
        void RandomizeObjects();
        void DrawObjects();
        void DrawAllScene(float deltaTimeSeconds);


     protected:
        implemented::Camera *camera;
        glm::mat4 projectionMatrix;
        bool renderCameraTarget;
        float fov;

        float gap = 0.0f;

		glm::vec3 central_station_color = glm::vec3( 0.52f, 0.52f, 0.52f);
		glm::vec3 station_1_color = glm::vec3(1, 0, 0);
		glm::vec3 station_2_color = glm::vec3(0.3f, 0.3f, 0.8f);
		glm::vec3 station_3_color = glm::vec3(0.8f, 0.8f, 0.3f);
		glm::vec3 station_4_color = glm::vec3(0.3f, 0.8f, 0.3f);
        
        glm::vec3 rail_color = glm::vec3(1, 1, 1);
		glm::vec3 tunnel_color = glm::vec3(1, 0.87f, 0.68f);
        glm::vec3 bridge_color = glm::vec3(0.54f, 0.99f, 1);

        struct Rail {
            glm::vec3 startPosition;
			glm::vec3 endPosition;
            int type_rail;
			std::vector<int> branches_start;
            std::vector<int> branches_end;

			Rail(glm::vec3 start, glm::vec3 end, int type) : startPosition(start), endPosition(end), type_rail(type) {}
        };

        struct Train {
			int currentRailIndex;
            float progress;
            float speed;
        };

        struct Wagon {
            std::queue<int> railQueue;
            float progress;
            float speed;
        };
        
        Train train;
		Wagon wagon;
        std::vector<Rail> rails;

        int junctionIndex = 0;

        float timer;
        float time_to_complete_mission = 100.0f;
        int missions = 0;
        bool end_game;

        int station_1_items = 0;
        int station_2_items = 0;
        int station_3_items = 0;
        int station_4_items = 0;

        float timer1 = 0;
		float timer2 = 0;
        float timer3 = 0;
        float timer4 = 0;
		float respawn_time = 5.0f;

        glm::vec3 item1_pos = glm::vec3(-19, 0, 24);
        glm::vec3 item2_pos = glm::vec3(21, 0, -23);
        glm::vec3 item3_pos = glm::vec3(-22, 0, -23);
        glm::vec3 item4_pos = glm::vec3(21.4f, 0, 23);
        glm::vec3 done_mission = glm::vec3(0, 0, 1);

        bool cameraFollowing = false;
        float cameraOffset = 2.0f;
		float cameraOffsetY = 2.0f;

        struct ViewportArea
        {
            ViewportArea() : x(0), y(0), width(1), height(1) {}
            ViewportArea(int x, int y, int width, int height)
                : x(x), y(y), width(width), height(height) {
            }
            int x;
            int y;
            int width;
            int height;
        };
		ViewportArea miniViewportArea;

        gfxc::TextRenderer* textRenderer;
    };
}   // namespace m1
