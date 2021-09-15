// FluidSim2D.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "FPSLimiter.h"
#include "ImpulseState.h"
#include "Shader.h"

#define NOMINMAX
#include <Windows.h>

extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

// Settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;
// All bits except for last (even from odd)
constexpr std::size_t NumJacobiRounds{30 & ~0x1};

// Identifiers
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

static std::string LoadShader(std::string_view name)
{
    std::ifstream file;
	file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	file.open(name.data());

    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
};

static void APIENTRY DebugMessageCallback(const GLenum source, const GLenum type, const GLuint id, const GLenum severity, const GLsizei length, const GLchar *const message, const void *const userParam)
{
    std::ostringstream msg;
    msg << "source: " << source << ", type: " << type << ", id: " << id << ", severity: " << severity << ", message: " << std::string{ reinterpret_cast<const char* const>(message), static_cast<std::size_t>(length) } << "\n";
	OutputDebugStringA(msg.str().c_str());
}

class CStdRectangle : public CStdVAOObject<CStdRectangle>
{
public:
    static constexpr inline auto Dimensions = 3;
    static constexpr inline auto PrimitiveType = GL_TRIANGLES;

public:
	CStdRectangle() : CStdVAOObject{} { Init(); }

protected:
	virtual void GenerateGeometry(std::vector<GLfloat> &vertices, std::vector<GLuint> &elements, std::vector<GLfloat> &normals, std::vector<GLfloat> &textureCoordinates) override
	{
		vertices = {
		-1.0f, -1.0f, 0.0f,   // top left
		 1.0f, -1.0f, 0.0f,  // bottom left
		 1.0f,  1.0f, 0.0f,  // bottom right
		-1.0f,  1.0f, 0.0f,  // top right
		};

		elements = {  // note that we start from 0!
			0, 1, 2,   // first triangle
			2, 3, 0    // second triangle
		};

		textureCoordinates = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f
		};
	}
};

class CStdLine : public CStdVAOObject<CStdLine>
{
public:
    static constexpr inline auto Dimensions = 3;
    static constexpr inline auto PrimitiveType = GL_LINES;

public:
	CStdLine(const glm::vec2 &start, const glm::vec2 &end) : CStdVAOObject{}, start{start}, end{end} { Init(); }

protected:
	virtual void GenerateGeometry(std::vector<GLfloat> &vertices, std::vector<GLuint> &elements, std::vector<GLfloat> &normals, std::vector<GLfloat> &textureCoordinates) override
	{
        vertices = {start.x, start.y, 0, end.x, end.y, 0};
        elements = {0, 1};
    }

private:
    glm::vec2 start;
    glm::vec2 end;
};

class MainProgram
{
    struct Border
    {
        CStdLine top;
        CStdLine left;
        CStdLine bottom;
        CStdLine right;
    };

public:
    MainProgram(GLFWwindow *const window, const std::int32_t width, const int32_t height) 
        : window{window},
          width{width},
          height{height},
          gridScale{1.0f / width, 1.0f / height},
          velocityBuffer{width, height},
          pressureBuffer{width, height},
          vorticityBuffer{width, height},
          temporaryBuffer{width, height},
          border{InitBorder()},
		  limiter{FPS}
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }

public:
    void CopyBuffers(const CStdFramebuffer &source, const CStdFramebuffer &destination);
    void DrawQuad();
    void Load2DShaders();
    void Run();
    void ProcessInput();
    void DoDroplets();
    void SetBounds(float scale);

private:
    Border InitBorder();
    void BindTexture(CStdGLShaderProgram &program, const std::string &key, const CStdTexture &texture, GLuint offset);
    void SolvePoissonSystem(CStdSwappableFramebuffer &swappableBuffer, const CStdFramebuffer &initialValue, float alpha, float beta);
    glm::vec2 RandomPosition() const;

private:
    CStdGLShaderProgram advectShaderProgram;
    CStdGLShaderProgram addImpulseShaderProgram;
	CStdGLShaderProgram addRadialImpulseShaderProgram;
    CStdGLShaderProgram vorticityShaderProgram;
    CStdGLShaderProgram addVorticityShaderProgram;
    CStdGLShaderProgram jacobiShaderProgram;
    CStdGLShaderProgram divergenceShaderProgram;
    CStdGLShaderProgram gradientShaderProgram;
    CStdGLShaderProgram subtractShaderProgram;
    CStdGLShaderProgram boundaryShaderProgram;
    CStdGLShaderProgram copyShaderProgram;
    CStdGLShaderProgram renderShaderProgram;

    GLFWwindow *window;
    int32_t width;
    int32_t height;
    glm::vec2 gridScale;
    CStdRectangle quad;
    CStdSwappableFramebuffer velocityBuffer;
    CStdSwappableFramebuffer pressureBuffer;
    CStdFramebuffer vorticityBuffer;
    CStdFramebuffer temporaryBuffer;
    Border border;
    float dt;
    ImpulseState impulseState;
    static constexpr inline int FPS{ 60 };
    FPSLimiter limiter;
};

// Copies frameBuffer from source to destination
void MainProgram::CopyBuffers(const CStdFramebuffer &source, const CStdFramebuffer &destination)
{
    destination.Bind();
    copyShaderProgram.Select();
	BindTexture(copyShaderProgram, "field", source.GetTexture(), 0);
    DrawQuad();
}

void MainProgram::DrawQuad()
{
    quad.Bind();
    quad.Draw();
}

void MainProgram::Load2DShaders()
{
	CStdGLShader texCoordsShader{CStdShader::Type::Vertex, LoadShader("../Shader/tex_coords.vert")};
	texCoordsShader.Compile();

	const auto newShader = [this, &texCoordsShader](CStdGLShaderProgram &shaderProgram, std::string_view objectLabel)
	{
		CStdGLShader shader{CStdShader::Type::Fragment, LoadShader(std::string{"../Shader/"} + objectLabel.data() + ".frag")};
		shader.Compile();

		shaderProgram.AddShader(&texCoordsShader);
		shaderProgram.AddShader(&shader);
		shaderProgram.Link();
		shaderProgram.SetObjectLabel(objectLabel);

		glm::vec2 s{gridScale};
		shaderProgram.Select();
		shaderProgram.SetUniform("stride", gridScale);
	};

	newShader(advectShaderProgram, "advection");
	newShader(addImpulseShaderProgram, "add_impulse");
	newShader(addRadialImpulseShaderProgram, "add_radial_impulse");
	newShader(vorticityShaderProgram, "vorticity");
	newShader(addVorticityShaderProgram, "add_vorticity");
	newShader(jacobiShaderProgram, "jacobi");
	newShader(divergenceShaderProgram, "divergence");
	newShader(gradientShaderProgram, "gradient");
	newShader(subtractShaderProgram, "subtract");
	newShader(boundaryShaderProgram, "boundary");
	newShader(copyShaderProgram, "copy");

    CStdGLShader vertexShader{ CStdShader::Type::Vertex, LoadShader("../Shader/vertexShader.glsl") };
    vertexShader.Compile();

    CStdGLShader fragmentShader{ CStdShader::Type::Fragment, LoadShader("../Shader/fragmentShader.glsl") };
    fragmentShader.Compile();

    renderShaderProgram.AddShader(&vertexShader);
    renderShaderProgram.AddShader(&fragmentShader);
    renderShaderProgram.Link();
	renderShaderProgram.SetObjectLabel("render");
}

constexpr struct Variables
{
    float advectionDissipation;
    float gridScale;
    float vorticity;
    float viscosity;
    float splatRadius;
    bool droplets;
} vars{0.99f, 0.3f, 0.005f, 0.001f, 0.003f, false};

void MainProgram::Run()
{
    // Render loop
    while (!glfwWindowShouldClose(window))
	{
        double lastTime{0.0};
        double now{glfwGetTime()};
        glfwPollEvents();

        double timeStepEventPoll{glfwGetTime() - now};
        now = glfwGetTime();
        dt = lastTime == 0 ? 0.016667 : (now - lastTime) - timeStepEventPoll;
        lastTime = now;

        ProcessInput();

		if (vars.droplets)
        {
            DoDroplets();
		}

		glViewport(0, 0, width, height);

#pragma region Advection
        SetBounds(-1);

        velocityBuffer.GetBack().Bind();
        advectShaderProgram.Select();
        advectShaderProgram.SetUniform("dissipation", glUniform1f, vars.advectionDissipation);
        BindTexture(advectShaderProgram, "quantity", velocityBuffer.GetFront().GetTexture(), 1);
        advectShaderProgram.SetUniform("gs", glUniform1f, vars.gridScale);
        advectShaderProgram.SetUniform("rdv", gridScale);
        advectShaderProgram.SetUniform("delta_t", dt);
        BindTexture(advectShaderProgram, "velocity", velocityBuffer.GetFront().GetTexture(), 0);
        DrawQuad();

		velocityBuffer.SwapBuffers();
#pragma endregion

#pragma region Force Application
		if (impulseState.IsActive())
        {
            const auto diff = impulseState.Delta;
			const glm::vec3 force{ std::clamp(diff.x, -vars.gridScale, vars.gridScale), std::clamp(diff.y, -vars.gridScale, vars.gridScale), 0 };
			/*const glm::vec3 force(std::min(std::max(diff.x, -vars.gridScale), vars.gridScale),
						std::min(std::max(diff.y, -vars.gridScale), vars.gridScale),
						0);*/

            velocityBuffer.GetBack().Bind();

			CStdGLShaderProgram &program{impulseState.Radial ? addRadialImpulseShaderProgram : addImpulseShaderProgram};

			program.Select();
			program.SetUniform("position", glm::vec2{ impulseState.CurrentPos.x, impulseState.CurrentPos.y } * gridScale);
			program.SetUniform("radius", vars.splatRadius);
			BindTexture(program, "velocity", velocityBuffer.GetFront().GetTexture(), 0);

			if (!impulseState.Radial)
			{
				program.SetUniform("force", force);
			}

			program.SetUniform("delta_t", dt);

            DrawQuad();

            velocityBuffer.SwapBuffers();
		}
#pragma endregion

#pragma region Vorticity
        vorticityBuffer.Bind();
        vorticityShaderProgram.Select();
        vorticityShaderProgram.SetUniform("gs", glUniform1f, vars.gridScale);
        BindTexture(vorticityShaderProgram, "velocity", velocityBuffer.GetFront().GetTexture(), 0);
        DrawQuad();
#pragma endregion

        SetBounds(-1);

#pragma region Add Vorticity
        velocityBuffer.GetBack().Bind();
        addVorticityShaderProgram.Select();
        addVorticityShaderProgram.SetUniform("gs", glUniform1f, vars.gridScale);
        BindTexture(addVorticityShaderProgram, "velocity", velocityBuffer.GetFront().GetTexture(), 0);
        BindTexture(addVorticityShaderProgram, "vorticity", vorticityBuffer.GetTexture(), 1);
		addVorticityShaderProgram.SetUniform("delta_t", glUniform1f, 1.0f);
        addVorticityShaderProgram.SetUniform("scale", vars.vorticity);
        DrawQuad();
		velocityBuffer.SwapBuffers();
#pragma endregion

#pragma region Diffusion
        const float alpha{(vars.gridScale * vars.gridScale) / (vars.viscosity * dt)};
        const float beta{alpha + 4.0f};
        SolvePoissonSystem(velocityBuffer, velocityBuffer.GetFront(), alpha, beta);
#pragma endregion

#pragma region Projection
        // Calculate div(W)
        velocityBuffer.GetBack().Bind();
        divergenceShaderProgram.Select();
        divergenceShaderProgram.SetUniform("gs", glUniform1f, vars.gridScale);
        BindTexture(divergenceShaderProgram, "field", velocityBuffer.GetFront().GetTexture(), 0);
        DrawQuad();
		
        // Solve for P in: Laplacian(P) = div(W)
        SolvePoissonSystem(pressureBuffer, velocityBuffer.GetBack(), -vars.gridScale * vars.gridScale, 4.0f);
		
		// Calculate grad(P)
        pressureBuffer.GetBack().Bind();
        gradientShaderProgram.Select();
        gradientShaderProgram.SetUniform("gs", glUniform1f, vars.gridScale);
        BindTexture(gradientShaderProgram, "field", pressureBuffer.GetFront().GetTexture(), 0);
        DrawQuad();
		// No swap, back buffer has the gradient
        
        // Calculate U = W - grad(P) where div(U)=0
        velocityBuffer.GetBack().Bind();
        subtractShaderProgram.Select();
        BindTexture(subtractShaderProgram, "a", velocityBuffer.GetFront().GetTexture(), 0);
        BindTexture(subtractShaderProgram, "b", pressureBuffer.GetBack().GetTexture(), 1);
        DrawQuad();
        velocityBuffer.SwapBuffers();

        SetBounds(-1);
		
            
#pragma endregion


#pragma region Rendering
        velocityBuffer.Unbind();
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT);
        renderShaderProgram.Select();
        BindTexture(renderShaderProgram, "field", velocityBuffer.GetFront().GetTexture(), 0);
        DrawQuad();
#pragma endregion

        limiter.Regulate();

        glfwSwapBuffers(window);
    }
}

void MainProgram::ProcessInput()
{
    double cursorX;
    double cursorY;
    glfwGetCursorPos(window, &cursorX, &cursorY);
	impulseState.Update(cursorX, height - cursorY, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
}

void MainProgram::DoDroplets()
{
    static float acc{ 0.0f };
    static float nextDrop{ 0.0f };
    static glm::ivec2 frequencyRange{ 200, 1000 };

    acc += dt * 1000;
    if (acc >= nextDrop)
    {
        acc = 0;
        static constexpr auto Delay = 1000.0f;
        nextDrop = Delay + std::pow(-1, std::rand() % 2) * (std::rand() % static_cast<int>(0.5 * Delay));
        //LOG_INFO("Next drop: %.2f", next_drop);

        const glm::vec2 randomPositions[2]{  RandomPosition(), RandomPosition()  };
        impulseState.LastPos = glm::vec3{randomPositions[0].x, randomPositions[0].y, 0};
		impulseState.CurrentPos = glm::vec3{randomPositions[1].x, randomPositions[1].y, 0};
        impulseState.Delta = impulseState.CurrentPos - impulseState.LastPos;
        impulseState.ForceActive = true;
        impulseState.InkActive = true;
        impulseState.Radial = true;
    }
    else
    {
        impulseState.ForceActive = false;
        impulseState.InkActive = false;
        impulseState.Radial = false;
    }
}

void MainProgram::SetBounds(const float scale)
{
    CopyBuffers(velocityBuffer.GetFront(), velocityBuffer.GetBack());
    boundaryShaderProgram.Select();
    boundaryShaderProgram.SetUniform("rdv", gridScale);
	BindTexture(boundaryShaderProgram, "field", velocityBuffer.GetFront().GetTexture(), 0);
    boundaryShaderProgram.SetUniform("scale", glUniform1f, scale);

    velocityBuffer.GetBack().Bind();

    static constexpr glm::vec2 Top{0, -1};
    static constexpr glm::vec2 Left{1, 0};
    static constexpr glm::vec2 Bottom{0, 1};
    static constexpr glm::vec2 Right{-1, 0};

    boundaryShaderProgram.SetUniform("offset", Top);
    border.top.Bind();
    border.top.Draw();

    boundaryShaderProgram.SetUniform("offset", Left);
    border.left.Bind();
    border.left.Draw();

    boundaryShaderProgram.SetUniform("offset", Bottom);
    border.bottom.Bind();
    border.bottom.Draw();

    boundaryShaderProgram.SetUniform("offset", Right);
    border.right.Bind();
    border.right.Draw();

    velocityBuffer.SwapBuffers();
}

auto MainProgram::InitBorder() -> Border
{
    const glm::vec2 c{1.0f - 0.5f / width, 1.0f - 0.5f / height};

    return Border
    {
        {{-c.x, -c.y}, { c.x, -c.y}},
        {{-c.x,  c.y}, {-c.x, -c.y}},
        {{ c.x,  c.y}, {-c.x,  c.y}},
		{{ c.x, -c.y}, { c.x,  c.y}}
    };
}

void MainProgram::BindTexture(CStdGLShaderProgram &program, const std::string &key, const CStdTexture &texture, GLuint offset)
{
    program.SetUniform(key, glUniform1i, offset);
    texture.Bind(offset);
}

void MainProgram::SolvePoissonSystem(CStdSwappableFramebuffer &swappableBuffer, const CStdFramebuffer &initialValue, float alpha, float beta)
{
    CopyBuffers(initialValue, temporaryBuffer);
    jacobiShaderProgram.Select();
    jacobiShaderProgram.SetUniform("alpha", glUniform1f, alpha);
    jacobiShaderProgram.SetUniform("beta", glUniform1f, beta);
    BindTexture(jacobiShaderProgram, "b", temporaryBuffer.GetTexture(), 1);

    for (std::size_t i{0}; i < NumJacobiRounds; ++i)
    {
        swappableBuffer.GetBack().Bind();
        BindTexture(jacobiShaderProgram, "x", swappableBuffer.GetFront().GetTexture(), 0);
        DrawQuad();
        swappableBuffer.SwapBuffers();
    }
}

int main()
{
    // GLFW init and config
    // ---------------------------------
    struct GLFW { GLFW() { glfwInit(); } ~GLFW() { glfwTerminate(); }} glfw;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    // ---------------------------------
    
    // GLFW window creation
    // ---------------------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "FluidSim2D", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    //glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // ---------------------------------

    // GLAD OpenGL function pointers loader
    // ---------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    // ---------------------------------

    
    // Grid setup
    // ---------------------------------

    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(DebugMessageCallback, nullptr);

	MainProgram mainProgram{window, SCR_WIDTH + 2, SCR_HEIGHT + 2};
    mainProgram.Load2DShaders();
    mainProgram.Run();

    return 0;
    // ---------------------------------
}

glm::vec2 MainProgram::RandomPosition() const
{
    return { std::rand() % width, std::rand() % height };
}

// GLFW - Window size change callback function
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// GLFW - Input handler
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
