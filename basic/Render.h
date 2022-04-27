#ifndef RENDER_H
#define RENDER_H
#define GLM_SWIZZLE
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "scene.h"
#include "light.h"
#include "vector"
#include "shadow.h"
#include "GameController.h"
#include "gbuffer.h"
#include "deferredshading.h"

namespace KooNan
{
	class Render
	{
	private:
		Scene &main_scene;
		Light &main_light;
		Water_Frame_Buffer &waterfb;
		PickingTexture &mouse_picking;
		Shadow_Frame_Buffer &shadowfb;
		GBuffer gbuf;

	public:
		Render(Scene &main_scene, Light &main_light, Water_Frame_Buffer &waterfb, PickingTexture &mouse_picking, Shadow_Frame_Buffer &shadowfb) : main_scene(main_scene), main_light(main_light), waterfb(waterfb), mouse_picking(mouse_picking), shadowfb(shadowfb)
		{
			InitLighting(main_scene.TerrainShader);
			InitLighting(main_scene.WaterShader);
		}

		void DrawReflection(Shader &modelShader)
		{
#ifdef DEFERRED_SHADING

#else
			InitLighting(main_scene.TerrainShader);

			glm::vec4 clipping_plane = glm::vec4(0.0, 1.0, 0.0, -main_scene.getWaterHeight());
			glEnable(GL_CLIP_DISTANCE0);
			waterfb.bindReflectionFrameBuffer();
			glEnable(GL_DEPTH_TEST);
			// render
			// ------
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Set the main camera to the position and direction of reflection
			float distance = 2 * (GameController::mainCamera.Position.y - main_scene.getWaterHeight());
			GameController::mainCamera.Position.y -= distance;
			GameController::mainCamera.Pitch = -GameController::mainCamera.Pitch;

			DrawObjectsWithShading(modelShader, clipping_plane, false);

			// we now draw as many light bulbs as we have point lights.
			main_light.Draw(GameController::mainCamera, clipping_plane);
			// render the main scene
			main_scene.DrawSceneWithShading(GameController::deltaTime, GameController::mainCamera, clipping_plane, false);
			main_scene.DrawSky(GameController::mainCamera);

			// Restore the main camera
			GameController::mainCamera.Position.y += distance;
			GameController::mainCamera.Pitch = -GameController::mainCamera.Pitch;
			GameController::mainCamera.GetViewMatrix();

			waterfb.unbindCurrentFrameBuffer();
#endif
		}

		void DrawRefraction(Shader &modelShader)
		{
#ifdef DEFERRED_SHADING

#else
			InitLighting(main_scene.TerrainShader);

			glm::vec4 clipping_plane = glm::vec4(0.0, -1.0, 0.0, main_scene.getWaterHeight());
			glEnable(GL_CLIP_DISTANCE0);
			waterfb.bindRefractionFrameBuffer();
			glEnable(GL_DEPTH_TEST);
			// render
			// ------
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			DrawObjectsWithShading(modelShader, clipping_plane, false);

			// render the main scene
			main_scene.DrawSceneWithShading(GameController::deltaTime, GameController::mainCamera, clipping_plane, false);
			main_scene.DrawSky(GameController::mainCamera);

			waterfb.unbindCurrentFrameBuffer();
#endif
		}

		void DrawAll(Shader &pickingShader, Shader &modelShader, Shader &shadowShader)
		{
#ifdef DEFERRED_SHADING
			// Geometry pass
			glm::vec4 clipping_plane = glm::vec4(0.0, -1.0, 0.0, 99999.0f);

			glDisable(GL_BLEND);
			gbuf.bindToWrite();

			glEnable(GL_DEPTH_TEST);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			main_scene.DrawSceneWithoutShading(GameController::deltaTime, GameController::mainCamera, clipping_plane, false);
			DrawObjectsWithoutShading(modelShader, clipping_plane, false);

			// Lighting pass
			DeferredShading::lightingShader->use();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			gbuf.bindTexture();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			InitLighting(*DeferredShading::lightingShader);

			DeferredShading::DrawQuad(GameController::mainCamera);
			// Copy zbuffer to default framebuffer
			gbuf.bindToRead();
			glBlitFramebuffer(0, 0, Common::SCR_WIDTH, Common::SCR_HEIGHT, 0, 0, Common::SCR_WIDTH, Common::SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			// Draw things that do not need to be lit
			main_scene.DrawSky(GameController::mainCamera);
			main_light.Draw(GameController::mainCamera, clipping_plane);

#else
			InitLighting(main_scene.WaterShader);
			InitLighting(main_scene.TerrainShader);

			glm::vec4 clipping_plane = glm::vec4(0.0, -1.0, 0.0, 99999.0f);

			// ����ʰȡ
			if (GameController::gameMode == GameMode::Creating)
				if (GameController::creatingMode == CreatingMode::Selecting)

				{
					mouse_picking.bindFrameBuffer();
					glEnable(GL_DEPTH_TEST);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					PickObjects(pickingShader);
					mouse_picking.unbindFrameBuffer();
				}
				else
					;
			else
				GameController::selectedGameObj = NULL;

			// render
			// ------
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			DrawShadowMap(shadowShader);

			DrawObjectsWithShading(modelShader, clipping_plane, true);
			main_light.Draw(GameController::mainCamera, clipping_plane);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			main_scene.setReflectText(waterfb.getReflectionTexture());
			main_scene.setRefractText(waterfb.getRefractionTexture());
			main_scene.setDepthMap(waterfb.getRefractionDepthTexture());
			main_scene.setShadowMap(shadowfb.getShadowTexture());
			main_scene.TerrainShader.use();
			main_scene.TerrainShader.setMat4("lightProjection", shadowfb.lightProjection); // Bad implementation
			main_scene.TerrainShader.setMat4("lightView", shadowfb.lightView);			   // Bad implementation
			main_scene.DrawSceneWithShading(GameController::deltaTime, GameController::mainCamera, clipping_plane, true, true);
			main_scene.DrawSky(GameController::mainCamera);

			glDisable(GL_BLEND);
#endif
		}

	private:
		void InitLighting(Shader &shader)
		{
			main_light.SetLight(shader);
		}

		void DrawObjectsWithoutShading(Shader &modelShader, glm::vec4 clippling_plane, bool IsAfterPicking)
		{
			glEnable(GL_CULL_FACE);
			auto itr = GameObject::gameObjList.begin();
			for (int i = 0; i < GameObject::gameObjList.size(); i++, ++itr)
			{
				(*itr)->Draw(modelShader, GameController::mainCamera.Position,
							 Common::GetPerspectiveMat(GameController::mainCamera), GameController::mainCamera.GetViewMatrix(),
							 clippling_plane,
							 false);
			}
			glDisable(GL_CULL_FACE);
		}
		void DrawObjectsWithShading(Shader &modelShader, glm::vec4 clippling_plane, bool IsAfterPicking)
		{
			InitLighting(modelShader);
			glEnable(GL_CULL_FACE);
			bool enablePicking = GameController::gameMode == GameMode::Creating &&
								 GameController::creatingMode == CreatingMode::Selecting &&
								 IsAfterPicking && !GameController::isCursorOnGui;
			if (enablePicking)
				GameController::selectedGameObj = NULL;

			auto itr = GameObject::gameObjList.begin();
			for (int i = 0; i < GameObject::gameObjList.size(); i++, ++itr)
			{
				bool intersected = false;
				// ����ģʽ�ķ���ģʽ����û������ѡ�У��Ҹ�������Ա�ѡ�У�����ʰȡ
				if (enablePicking)
				{
					float hitObjID = mouse_picking.ReadPixel(GameController::cursorX,
															 Common::SCR_HEIGHT - GameController::cursorY)
										 .ObjID; // deviation of y under resolution 1920*1080 maybe 22
					if ((unsigned int)hitObjID == i + 1)
					{
						GameController::selectedGameObj = *itr;
						intersected = true;
					}
				}
				(*itr)->Draw(modelShader, GameController::mainCamera.Position,
							 Common::GetPerspectiveMat(GameController::mainCamera), GameController::mainCamera.GetViewMatrix(),
							 clippling_plane,
							 intersected);
			}
			glDisable(GL_CULL_FACE);
		}

		void PickObjects(Shader &modelShader)
		{
			glEnable(GL_CULL_FACE);
			unsigned int object_counter = 0;
			auto itr = GameObject::gameObjList.begin();
			for (int i = 0; i < GameObject::gameObjList.size(); i++, ++itr)
			{
				if ((*itr)->IsPickable)
					(*itr)->Pick(modelShader, ++object_counter, 0,
								 Common::GetPerspectiveMat(GameController::mainCamera), GameController::mainCamera.GetViewMatrix());
			}
			glDisable(GL_CULL_FACE);
		}

		void DrawShadowMap(Shader &shadowShader)
		{
			Camera &cam = GameController::mainCamera;
			glm::vec3 LightDir = main_light.GetDirLightDirection() * 10.0f;
			glm::vec3 DivPos = cam.Position;
			DivPos.z -= 20.0f;
			GLfloat near_plane = 1.0f, far_plane = 1000.0f;
			shadowfb.lightProjection = glm::ortho(-50.0f, 50.0f, -80.0f, 20.0f, near_plane, far_plane);
			// glm::lookat(eye, center, up)
			shadowfb.lightView = glm::lookAt(DivPos - LightDir, DivPos, glm::vec3(0.0f, 1.0f, 0.0f));

			Common::setWidthAndHeight();
			glViewport(0, 0, shadowfb.SHADOW_WIDTH, shadowfb.SHADOW_HEIGHT);
			shadowfb.bindFrameBuffer();
			glClear(GL_DEPTH_BUFFER_BIT);
			auto itr = GameObject::gameObjList.begin();
			for (int i = 0; i < GameObject::gameObjList.size(); i++, ++itr)
			{
				(*itr)->Draw(shadowShader, cam.Position, shadowfb.lightProjection, shadowfb.lightView);
			}
			shadowfb.unbindFrameBuffer();

			Common::setWidthAndHeight();
			glViewport(0, 0, Common::SCR_WIDTH, Common::SCR_HEIGHT);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
	};
}
#endif
