#include "OpenGL/renderer.h"
#include "OpenGL/camera.h"

#include "World/things.h"
#include "World/mesh.h"

#include "Modules/font.h"
#include "Modules/bitmap.h"
#include "Modules/fontAtlas.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <functional>

// function declarations ------------------------------------------------------
void processInput(GLFWwindow* window);

void framebuffer_size_callback(GLFWwindow*, int, int);
void mouse_callback(GLFWwindow* window, double xPos, double yPos);
void scroll_callback(GLFWwindow* window, double xOffset, double yOffset);

void RenderText(Shader&, std::string, float, float, float, glm::vec3);
void RenderGraph(std::function<float(float)> func, float, float, float, float, float, float);

// global variables -----------------------------------------------------------
const int screenWidth = 600, screenHeight = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
float lastX = screenWidth / 2, lastY = screenHeight / 2;
float fov = 45.0;
bool firstMouse = true;

struct Character{
	Glyph ftGlyph;
};

std::map<char, Character> characters;
std::unique_ptr<VertexArray> fontVAO;
std::unique_ptr<VertexBuffer> fontVBO;
std::unique_ptr<IndexBuffer> fontIBO;
std::unique_ptr<FontAtlas> fontAtlas;

bool textBlending = true;

int main() {
	// glfw: initialize and configure --------------------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	glfwWindowHint(GLFW_SAMPLES, 16); // MSAA subsample number

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw: window creation and setup -------------------------------------------
	GLFWwindow* window = glfwCreateWindow
		(screenWidth,  screenHeight, "OpenGL LittleBigEngine", NULL, NULL);
	if (!window) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	// glfw: setup
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// engine initialization -----------------------------------------------------
	// -> error texture fallback
	Texture& _error_texture = Things::LoadTexture(LBE_ERROR_TEXTURE_NAME, LBE_ERROR_TEXTURE_PATH, true);
	_error_texture.SetPar(GL_TEXTURE_WRAP_S, LBE_DEFAULT_TEXTURE_WRAP_S);
	_error_texture.SetPar(GL_TEXTURE_WRAP_T, LBE_DEFAULT_TEXTURE_WRAP_T);
	_error_texture.SetPar(GL_TEXTURE_MIN_FILTER, LBE_DEFAULT_TEXTURE_MIN_FILTER);
	_error_texture.SetPar(GL_TEXTURE_MAG_FILTER, LBE_DEFAULT_TEXTURE_MAG_FILTER);

	// -> font loading
	Font timesNewRoman("res/fonts/Times-New-Roman.ttf", 0, 32);
	fontAtlas = std::make_unique<FontAtlas>(timesNewRoman);

	glm::mat4 fontModel = glm::mat4(1.0f);
	glm::mat4 fontView = glm::mat4(1.0f);
	glm::mat4 fontProjection = glm::ortho(
		0.0f, static_cast<float>(screenHeight),
		0.0f, static_cast<float>(screenWidth));
	
	fontVAO = std::make_unique<VertexArray>();
	fontVBO = std::make_unique<VertexBuffer>(nullptr, sizeof(float) * 4 * 4, GL_DYNAMIC_DRAW);
	VertexBufferLayout fontVBL;
	fontVBL.Push<float>(4);
	fontVAO->AddBuffer(*fontVBO, fontVBL);

	unsigned int fontIndices[] = {
		0, 1, 2,
		0, 2, 3
	};
	fontIBO = std::make_unique<IndexBuffer>(fontIndices, 6);

	// vertices definition -------------------------------------------------------

	// Model (vertex and buffers) configurations ---------------------------------

	// texture handling ----------------------------------------------------------
	const Bitmap& atlasBMP = fontAtlas->GetBitmap();
	fontAtlas->ExportBitmapAtlas("res/bitmap/timesNewRomanAtlas.bmp");
	
	Texture atlasTexture(GL_TEXTURE_2D, GL_RED);
	atlasTexture.Bind();
	atlasTexture.SetBitUnpackingSize(1);
	atlasTexture.SetPar(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	atlasTexture.SetPar(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	atlasTexture.SetPar(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	atlasTexture.SetPar(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	atlasTexture.Load(atlasBMP.GetRawData(), atlasBMP.GetWidth(), atlasBMP.GetRows());

	// initialization before rendering -------------------------------------------
	Shader& fontShader = Things::LoadShader(
		"font2D",
		"res/shaders/main2D.vert",
		"res/shaders/font2D.frag");
	
	fontShader.SetUniform("projection", fontProjection);
	fontShader.SetUniform("model", fontModel);

	//glEnable(GL_MULTISAMPLE); // Enabled by default
	glLineWidth(3);
	//glEnable(GL_LINE_SMOOTH);

	// render loop (happens every frame) -----------------------------------------
	while (!glfwWindowShouldClose(window)) {
		// -> frame time tracker
		Renderer::FrameTimeTracker();
		
		// -> input handling
		processInput(window);
		// --> space configurations and rendering
		Renderer::SetRender3D(true);
		Renderer::RenderConfig(0.4f, 0.4f, 0.4f);
		
		// 3D rendering specifics
		glEnable(GL_DEPTH_TEST);
		// ---> world config
		// ---> camera config
		// ---> model positioning

		// 2D rendering specifics (Preferentially orthogonal rendering)
		glDisable(GL_DEPTH_TEST);

		// ---> font rendering
		Renderer::SetRender3D(false);
		bool shouldBlend = textBlending;
		if (shouldBlend) glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glActiveTexture(GL_TEXTURE0);
		atlasTexture.Bind();
		RenderText(fontShader, "graphing_calculator_v0.1", 40.0f, 10.0f, .5f, glm::vec3(1.0f, 1.0f, 0.0f));
		RenderText(fontShader, "0,0", 10.0f, 10.0f, .5f, glm::vec3(1.0f, 1.0f, 1.0f));
		RenderText(fontShader, "1,1", screenWidth - 30.0f, screenHeight - 20.0f, .5f, glm::vec3(1.0f, 1.0f, 1.0f));
		RenderText(fontShader, "0,1", 10.0f, screenHeight - 20.0f, .5f, glm::vec3(1.0f, 1.0f, 1.0f));
		RenderText(fontShader, "1,0", screenWidth - 30.0f, 10.0f, .5f, glm::vec3(1.0f, 1.0f, 1.0f));
		if (shouldBlend) glDisable(GL_BLEND);

		// ---> graph render
		RenderGraph([](float x){return x*x*x;}, -10, 10, -10, 10, 10, 10);
		RenderGraph([](float x){return std::sin(x);}, -10, 10, -10, 10, 10, 10);
		RenderGraph([](float x){return std::cos(x);}, -10, 10, -10, 10, 10, 10);
		// RenderGraph([](float x){return x!=0?1/x:0;}, -10, 10, -10, 10, 10, 10);

		// -> check and call events and swap the buffers
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

void processInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) textBlending = false;
	else textBlending = true;

	const float cameraSpeed = 2.5f * Renderer::GetDeltaTime();
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, Renderer::GetDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, Renderer::GetDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, Renderer::GetDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, Renderer::GetDeltaTime());
}

void mouse_callback(GLFWwindow* window, double xPos, double yPos) {
	if (firstMouse) {
		lastX = xPos;
		lastY = yPos;
		firstMouse = false;
	}

	float xOffset = xPos - lastX;
	float yOffset = lastY - yPos;

	lastX = xPos;
	lastY = yPos;

	camera.ProcessMouseMovement(xOffset, yOffset);
}

void scroll_callback(GLFWwindow* window, double xOffset, double yOffset) {
	camera.ProcessMouseScroll(yOffset);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

void RenderText(Shader& fs, std::string text, float x, float y, float scale, glm::vec3 color){
	fs.SetUniform("textColor", color);
	fontVAO->Bind();

	std::string::const_iterator c;
	for (c = text.begin(); c != text.end(); c++){
		const CellData& charCellData = fontAtlas->GetCharacterData(*c);
		const GlyphMetrics& characterData = charCellData.glyphMetrics;

		float posX = x + (characterData.xBearing * scale);
		float posY = y - ((characterData.rows - characterData.yBearing) * scale);

		float w = characterData.width * scale;
		float z = characterData.rows * scale;

		float u = static_cast<float>(charCellData.xAtlasOffset) / fontAtlas->GetBitmap().GetWidth();
		float v = static_cast<float>(charCellData.yAtlasOffset) / fontAtlas->GetBitmap().GetRows();

		float u1 = static_cast<float>(characterData.width) / fontAtlas->GetBitmap().GetWidth();
		float v1 = static_cast<float>(characterData.rows) / fontAtlas->GetBitmap().GetRows();

		//update VBO for each character
		float vertices[4][4] = {
			{ posX,		posY + z,	u,		v },
			{ posX,		posY,		u,		v + v1},
			{ posX + w,	posY,		u + u1,	v + v1},
			{ posX + w,	posY + z,	u + u1,	v}
		};

		// render glyph texture over quad
		fontVBO->Update(vertices, sizeof(vertices), 0);
		Renderer::Render(*fontVAO, *fontIBO, fs);

		x += characterData.xAdvance * scale;
	}
}

void RenderGraph(std::function<float(float)> func,
	float minX, float maxX,
	float minY, float maxY, 
	float subdivisionsX, float subdivisionsY)
{
	Shader&	funcShader = Things::LoadShader(
		"funcShader2D",
		"res/shaders/main2D.vert",
		"res/shaders/graph2D.frag");
		
	glm::mat4 funcModel = glm::mat4(1.0f);
	glm::mat4 funcProjection = glm::ortho(
		0.0f, static_cast<float>(screenHeight),
		0.0f, static_cast<float>(screenWidth), -1.0f, 1.0f);

	funcShader.SetUniform("projection", funcProjection);
	funcShader.SetUniform("model", funcModel);

	glm::vec3 lineColor(0.0f, 0.0f, .5f);
	funcShader.SetUniform("textColor", lineColor);

	float offset = 0.0f;
	if (screenWidth > screenHeight)
		offset = screenHeight / 2;
	else
		offset = screenWidth / 2;

	float xWidth = (maxX - minX);
	float yWidth = (maxY - minY);

	float xStretch = screenWidth / xWidth;
	float yStretch = screenHeight / yWidth;

	float precision = subdivisionsX * 100 + 1;
	float xStep = xWidth / precision;
	float xCnt = minX;

	std::vector<float> coords;

	glm::vec2 lastVec(0.0f, 0.0f);
	for (int i = 0; i < precision + 1; i++){
		float x = xCnt * xStretch + offset;		// Screen coordinates adjust for x
		float y = func(xCnt) * yStretch + (screenHeight - offset);	// Screen coordinates adjust for y
		xCnt += xStep;
		
		coords.emplace_back(x);
		coords.emplace_back(y);
	}

	// ------------ Slope ------------
	VertexArray funcVA;
	funcVA.Bind();
	VertexBuffer funcVBO(coords.data(), sizeof(float) * coords.size(), GL_STATIC_DRAW);

	VertexBufferLayout funcVBL;
	funcVBL.Push<float>(2); // X and Y values for every point
	funcVA.AddBuffer(funcVBO, funcVBL);

	funcShader.Use();
	//GL_LINES only selects pair of points, strip connects them
	glDrawArrays(GL_LINE_STRIP, 0, coords.size()/2);

	// ------------ Grid lines ------------
	glm::vec3 gridColor(0.0f);

	// Main grid
	funcShader.SetUniform("textColor", gridColor);
	std::vector<float> mainHorizontalGrid = {
		0.0f, screenHeight - screenWidth/2,
		screenWidth, screenHeight - screenWidth/2
	};

	VertexArray gridVA;
	gridVA.Bind();
	VertexBuffer gridVBO(mainHorizontalGrid.data(), sizeof(float) * mainHorizontalGrid.size(), GL_STATIC_DRAW);

	VertexBufferLayout gridVBL;
	gridVBL.Push<float>(2); // X and Y values for every point
	gridVA.AddBuffer(gridVBO, gridVBL);

	funcShader.Use();
	//GL_LINES only selects pair of points, strip connects them
	glDrawArrays(GL_LINE_STRIP, 0, mainHorizontalGrid.size()/2);
	// second grid

	// third grid
}