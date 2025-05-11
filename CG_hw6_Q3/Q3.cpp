#include <Windows.h>
#include <iostream>
#include <GL/glew.h>
#include <GL/GL.h>
#include <GL/freeglut.h>

#define GLFW_INCLUDE_GLU
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <vector>

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include "SceneGenerator.h"  // create_scene 사용

using namespace glm;

// -------------------------------------------------
// Global Variables
// -------------------------------------------------
int Width = 512;
int Height = 512;
std::vector<float> OutputImage;
std::vector<float> zBuffer; // Z-buffer
// 퐁 쉐이딩 용
static std::vector<glm::vec3> gVSPosBuffer;
static std::vector<glm::vec3> gVSNorBuffer;
// -------------------------------------------------

glm::vec3 computeLighting(
	glm::vec3 pos, glm::vec3 normal,
	glm::vec3 viewDir,
	glm::vec3 ka, glm::vec3 kd, glm::vec3 ks,
	float p
) {
	glm::vec3 lightPos = glm::vec3(-4, 4, -3);
	glm::vec3 lightColor = glm::vec3(1.0f);
	float Ia = 0.2f;

	glm::vec3 lightDir = glm::normalize(lightPos - pos);
	glm::vec3 halfway = glm::normalize(lightDir + viewDir);

	float diff = glm::max(glm::dot(normal, lightDir), 0.0f);
	float spec = glm::pow(glm::max(glm::dot(normal, halfway), 0.0f), p);

	glm::vec3 ambient = Ia * ka;
	glm::vec3 diffuse = diff * kd;
	glm::vec3 specular = spec * ks;

	return ambient + diffuse + specular;
}

void putPixel(int x, int y, float z, glm::vec3 color) // z-buffer 테스트
{
	if (x < 0 || x >= Width || y < 0 || y >= Height) return;

	int idx = (y * Width + x);
	if (z < zBuffer[idx]) {
		zBuffer[idx] = z;
		idx *= 3;
		OutputImage[idx + 0] = color.r;
		OutputImage[idx + 1] = color.g;
		OutputImage[idx + 2] = color.b;
	}
}

void rasterizeTriangle(const glm::vec4& a, const glm::vec4& b, const glm::vec4& c, const glm::uvec3& tri) // 증분식 구현
{
	auto edgeFunc = [](const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) -> float {
		return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
		};

	glm::vec2 p0 = a.xy();
	glm::vec2 p1 = b.xy();
	glm::vec2 p2 = c.xy();
	float area = edgeFunc(p0, p1, p2);
	if (area == 0) return;

	// bounding box
	int minX = std::max(0, (int)std::floor(std::min({ p0.x, p1.x, p2.x })));
	int maxX = std::min(Width - 1, (int)std::ceil(std::max({ p0.x, p1.x, p2.x })));
	int minY = std::max(0, (int)std::floor(std::min({ p0.y, p1.y, p2.y })));
	int maxY = std::min(Height - 1, (int)std::ceil(std::max({ p0.y, p1.y, p2.y })));

	glm::vec3 p0_vs = gVSPosBuffer[tri.x],
		p1_vs = gVSPosBuffer[tri.y],
		p2_vs = gVSPosBuffer[tri.z];
	glm::vec3 n0_vs = gVSNorBuffer[tri.x],
		n1_vs = gVSNorBuffer[tri.y],
		n2_vs = gVSNorBuffer[tri.z];

	// Per-Pixel Phong Shading 루프
	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			glm::vec2 p(x + .5f, y + .5f);
			float w0 = edgeFunc(p1, p2, p);
			float w1 = edgeFunc(p2, p0, p);
			float w2 = edgeFunc(p0, p1, p);

			if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
				w0 /= area; w1 /= area; w2 /= area;

				// 1) 깊이 보간
				float z = w0 * a.z + w1 * b.z + w2 * c.z;

				// 2) 뷰스페이스 위치/노멀 보간
				glm::vec3 pos_vs = w0 * p0_vs + w1 * p1_vs + w2 * p2_vs;
				glm::vec3 normal_vs = glm::normalize(
					w0 * n0_vs + w1 * n1_vs + w2 * n2_vs
				);
				glm::vec3 viewDir = glm::normalize(-pos_vs);

				// 3) 픽셀 단위 조명 계산 → 감마
				glm::vec3 color = computeLighting(
					pos_vs, normal_vs, viewDir,
					glm::vec3(0, 1, 0),    // ka
					glm::vec3(0, 0.5f, 0), // kd
					glm::vec3(0.5f),     // ks
					32.0f                // shininess
				);
				color = glm::pow(color, glm::vec3(1.0f / 2.2f));

				putPixel(x, y, z, color);
			}
		}
	}
}

void render()
{
	//Create our image. We don't want to do this in 
	//the main loop since this may be too slow and we 
	//want a responsive display of our beautiful image.
	//Instead we draw to another buffer and copy this to the 
	//framebuffer using glDrawPixels(...) every refresh
	OutputImage.clear();
	OutputImage.assign(Width * Height * 3, 0.0f);
	zBuffer.assign(Width * Height, 1e30f);

	create_scene();

	// MVP 행렬 생성
	glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -7.0f));
	M = glm::scale(M, glm::vec3(2.0f)); // 반지름 2

	glm::vec3 u(1.0f, 0.0f, 0.0f);  // 오른쪽
	glm::vec3 v(0.0f, 1.0f, 0.0f);  // 위쪽
	glm::vec3 w(0.0f, 0.0f, 1.0f);  // 카메라 뒤쪽

	glm::vec3 e(0.0f, 0.0f, 0.0f);  // eye 위치

	glm::mat4 V(1.0f);
	V[0][0] = u.x; V[1][0] = u.y; V[2][0] = u.z; V[3][0] = -glm::dot(u, e);
	V[0][1] = v.x; V[1][1] = v.y; V[2][1] = v.z; V[3][1] = -glm::dot(v, e);
	V[0][2] = w.x; V[1][2] = w.y; V[2][2] = w.z; V[3][2] = -glm::dot(w, e);
	V[0][3] = 0.0f; V[1][3] = 0.0f; V[2][3] = 0.0f; V[3][3] = 1.0f;


	float l = -0.1f, r = 0.1f;
	float b = -0.1f, t = 0.1f;
	float n = -0.1f, f = -1000.0f;

	glm::mat4 P(0.0f);
	P[0][0] = 2 * n / (r - l);
	P[1][1] = 2 * n / (t - b);
	P[2][0] = (l + r) / (l - r);
	P[2][1] = (b + t) / (b - t);
	P[2][2] = (f + n) / (n - f);
	P[2][3] = 1.0f;
	P[3][2] = (2 * f * n) / (f - n);

	glm::mat4 MVP = P * V * M;

	gVSPosBuffer.clear();
	gVSNorBuffer.clear();
	gVSPosBuffer.reserve(gVertexBuffer.size());
	gVSNorBuffer.reserve(gVertexBuffer.size());

	for (size_t i = 0; i < gVertexBuffer.size(); ++i) {
		// 객체 공간 법선
		glm::vec3 n_obj = glm::normalize(gVertexBuffer[i]);

		// ① 뷰스페이스 법선 (M까지 적용, w=0)
		glm::vec3 normal_vs = glm::normalize(
			glm::vec3(V * M * glm::vec4(n_obj, 0.0f))
		);

		// ② 뷰스페이스 위치 (w=1)
		glm::vec3 pos_vs = glm::vec3(
			V * M * glm::vec4(gVertexBuffer[i], 1.0f)
		);

		gVSNorBuffer.push_back(normal_vs);
		gVSPosBuffer.push_back(pos_vs);
	}

	// 변환된 정점 리스트 만들기
	std::vector<glm::vec4> screenVertices;
	for (auto& v : gVertexBuffer)
	{
		glm::vec4 p = MVP * glm::vec4(v, 1.0f);
		p /= p.w;

		p.x = (p.x * 0.5f + 0.5f) * Width;
		p.y = (p.y * 0.5f + 0.5f) * Height;
		p.z = (p.z * 0.5f + 0.5f); // z도 0~1로 정규화

		screenVertices.push_back(p);
	}

	// 삼각형 rasterize
	for (auto& tri : gIndexBuffer)
	{
		glm::vec4 a = screenVertices[tri.x];
		glm::vec4 b = screenVertices[tri.y];
		glm::vec4 c = screenVertices[tri.z];

		// Backface culling
		// 화면 공간에서의 삼각형 법선 계산
		glm::vec2 a2(a.x, a.y);
		glm::vec2 b2(b.x, b.y);
		glm::vec2 c2(c.x, c.y);

		glm::vec2 ab = b2 - a2;
		glm::vec2 ac = c2 - a2;
		float faceNormal = ab.x * ac.y - ab.y * ac.x;

		// 카메라가 z-축 음 방향을 바라보고 있다고 가정
		if (faceNormal > 0) continue; // 시계 방향이면 백페이스 → 스킵

		rasterizeTriangle(a, b, c, tri);
	}
}




void resize_callback(GLFWwindow*, int nw, int nh)
{
	//This is called in response to the window resizing.
	//The new width and height are passed in so we make 
	//any necessary changes:
	Width = nw;
	Height = nh;
	//Tell the viewport to use all of our screen estate
	glViewport(0, 0, nw, nh);

	//This is not necessary, we're just working in 2d so
	//why not let our spaces reflect it?
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0.0, static_cast<double>(Width)
		, 0.0, static_cast<double>(Height)
		, 1.0, -1.0);

	//Reserve memory for our render so that we don't do 
	//excessive allocations and render the image
	OutputImage.reserve(Width * Height * 3);
	render();
}


int main(int argc, char* argv[])
{
	// -------------------------------------------------
	// Initialize Window
	// -------------------------------------------------

	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
		return -1;

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(Width, Height, "OpenGL Viewer", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	//We have an opengl context now. Everything from here on out 
	//is just managing our window or opengl directly.

	//Tell the opengl state machine we don't want it to make 
	//any assumptions about how pixels are aligned in memory 
	//during transfers between host and device (like glDrawPixels(...) )
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	//We call our resize function once to set everything up initially
	//after registering it as a callback with glfw
	glfwSetFramebufferSizeCallback(window, resize_callback);
	resize_callback(NULL, Width, Height);

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		//Clear the screen
		glClear(GL_COLOR_BUFFER_BIT);

		// -------------------------------------------------------------
		//Rendering begins!
		glDrawPixels(Width, Height, GL_RGB, GL_FLOAT, &OutputImage[0]);
		//and ends.
		// -------------------------------------------------------------

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();

		//Close when the user hits 'q' or escape
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS
			|| glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
