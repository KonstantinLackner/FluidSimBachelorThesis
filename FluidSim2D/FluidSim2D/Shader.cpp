#include "Shader.h"

void CStdShader::SetMacro(const std::string& key, const std::string& value)
{
	macros[key] = value;
}

void CStdShader::UnsetMacro(const std::string& key)
{
	macros.erase(key);
}

void CStdShader::SetSource(const std::string& source)
{
	this->source = source;
}

void CStdShader::AddInclude(const std::string& source)
{
	includes.emplace_back(source);
}

void CStdShader::SetType(Type type)
{
	this->type = type;
}

void CStdShader::Clear()
{
	source.clear();
	macros.clear();
	includes.clear();
	errorMessage.clear();
}

CStdShaderProgram* CStdShaderProgram::currentShaderProgram = nullptr;

bool CStdShaderProgram::AddShader(CStdShader* shader)
{
	EnsureProgram();
	if (std::find(shaders.cbegin(), shaders.cend(), shader) != shaders.cend())
	{
		return true;
	}

	if (AddShaderInt(shader))
	{
		shaders.push_back(shader);
		return true;
	}

	return false;
}

void CStdShaderProgram::Clear()
{
	shaders.clear();
}

void CStdShaderProgram::Select()
{
	if (currentShaderProgram != this)
	{
		OnSelect();
		currentShaderProgram = this;
	}
}

void CStdShaderProgram::Deselect()
{
	if (currentShaderProgram)
	{
		currentShaderProgram->OnDeselect();
		currentShaderProgram = nullptr;
	}
}

CStdShaderProgram* CStdShaderProgram::GetCurrentShaderProgram()
{
	return currentShaderProgram;
}

void CStdGLShader::Compile()
{
	if (shader) // recompiling?
	{
		glDeleteShader(shader);
		errorMessage.clear();
	}

	GLenum t;
	switch (type)
	{
	case Type::Vertex:
		t = GL_VERTEX_SHADER;
		break;

		/*
		case Type::TesselationControl:
			t = GL_TESS_CONTROL_SHADER;
			break;

		case Type::TesselationEvaluation:
			t = GL_TESS_EVALUATION_SHADER;
			break;
			*/
	case Type::Geometry:
		t = GL_GEOMETRY_SHADER;
		break;

	case Type::Fragment:
		t = GL_FRAGMENT_SHADER;
		break;

	case Type::Compute:
		t = GL_COMPUTE_SHADER;
		break;

	default:
		throw Exception{"Invalid shader type"};
	}

	shader = glCreateShader(t);
	if (!shader)
	{
		throw Exception{"Could not create shader"};
	}

	PrepareSource();

	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status)
	{
		GLint size = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
		if (size)
		{
			std::string errorMessage;
			errorMessage.resize(size);
			glGetShaderInfoLog(shader, size, NULL, errorMessage.data());
			throw Exception{errorMessage.c_str()};
		}

		throw Exception{"Unknown error"};
	}
}

void CStdGLShader::Clear()
{
	if (shader)
	{
		glDeleteShader(shader);
		shader = 0;
	}

	CStdShader::Clear();
}

void CStdGLShader::PrepareSource()
{
	size_t pos = source.find("#version");
	if (pos == std::string::npos)
	{
		glDeleteShader(shader);
		throw Exception{"Version directive must be first statement and may not be repeated"};
	}

	pos = source.find('\n', pos + 1);
	assert(pos != std::string::npos);

	std::string copy = source;
	std::string buffer = "";

	for (const auto& [key, value] : macros)
	{
		buffer.append("#define ");
		buffer.append(key);
		buffer.append(" ");
		buffer.append(value);
		buffer.append("\n");
	}

	for (const auto &include : includes)
	{
		buffer.append(include);
	}

	buffer.append("#line 1\n");

	copy.insert(pos + 1, buffer);

	const char* s = copy.c_str();
	glShaderSource(shader, 1, &s, nullptr);
	glCompileShader(shader);
}

void CStdGLShaderProgram::Link()
{
	EnsureProgram();

	glLinkProgram(shaderProgram);

	GLint status = 0;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
	if (!status)
	{
		GLint size = 0;
		glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &size);
		assert(size);
		if (size)
		{
			std::string errorMessage;
			errorMessage.resize(size);
			glGetProgramInfoLog(shaderProgram, size, NULL, errorMessage.data());
			throw Exception{errorMessage.c_str()};
		}

		throw Exception{"Unknown error"};
	}

	glValidateProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &status);
	if (!status)
	{
		GLint size = 0;
		glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &size);
		if (size)
		{
			errorMessage.resize(size);
			glGetProgramInfoLog(shaderProgram, size, NULL, errorMessage.data());
			throw Exception{errorMessage.c_str()};
		}

		throw Exception{"Unknown error"};
	}

	for (const auto& shader : shaders)
	{
		glDetachShader(shaderProgram, dynamic_cast<CStdGLShader*>(shader)->GetHandle());
	}

	shaders.clear();
}

void CStdGLShaderProgram::Clear()
{
	for (const auto& shader : shaders)
	{
		glDetachShader(shaderProgram, dynamic_cast<CStdGLShader*>(shader)->GetHandle());
	}

	if (shaderProgram)
	{
		glDeleteProgram(shaderProgram);
		shaderProgram = 0;
	}

	attributeLocations.clear();
	uniformLocations.clear();

	CStdShaderProgram::Clear();
}

void CStdGLShaderProgram::EnsureProgram()
{
	if (!shaderProgram)
	{
		shaderProgram = glCreateProgram();
	}
	assert(shaderProgram);
}

bool CStdGLShaderProgram::SetUniform(const std::string& key, const glm::vec2& value)
{
	return SetUniform(key, glUniform2fv, 1, glm::value_ptr(value));
}

bool CStdGLShaderProgram::SetUniform(const std::string& key, const glm::vec3& value)
{
	return SetUniform(key, glUniform3fv, 1, glm::value_ptr(value));
}

bool CStdGLShaderProgram::SetUniform(const std::string& key, const glm::vec4& value)
{
	return SetUniform(key, glUniform4fv, 1, glm::value_ptr(value));
}

bool CStdGLShaderProgram::SetUniform(const std::string& key, const glm::mat4& value)
{
	return SetUniform(key, glUniformMatrix4fv, 1, false, glm::value_ptr(value));
}

void CStdGLShaderProgram::SetObjectLabel(std::string_view label)
{
	glObjectLabel(GL_PROGRAM, shaderProgram, label.size(), label.data());
}

bool CStdGLShaderProgram::AddShaderInt(CStdShader* shader)
{
	if (auto* s = dynamic_cast<CStdGLShader*>(shader); s)
	{
		glAttachShader(shaderProgram, s->GetHandle());
		return true;
	}

	return false;
}

void CStdGLShaderProgram::OnSelect()
{
	assert(shaderProgram);
	glUseProgram(shaderProgram);
}

void CStdGLShaderProgram::OnDeselect()
{
	glUseProgram(GL_NONE);
}

void CStdRectangle::GenerateGeometry(std::vector<GLfloat> &vertices, std::vector<GLuint> &elements, std::vector<GLfloat> &normals, std::vector<GLfloat> &textureCoordinates)
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

CStdTexture::CStdTexture(const std::int32_t width, const std::int32_t height, const GLenum internalFormat, const GLenum format, const GLenum type, void *const data)
	: width{width}, height{height}, internalFormat{internalFormat}, format{format}, type{type}
{
	const auto target = GetTarget();

	glGenTextures(1, &texture);
	glBindTexture(target, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	SetData(data);
}

CStdTexture::~CStdTexture()
{
	if (texture)
	{
		glDeleteTextures(1, &texture);
	}
}

void CStdTexture::Bind(GLenum offset) const
{
	glActiveTexture(GL_TEXTURE0 + offset);

	glBindTexture(GetTarget(), texture);
}

void CStdTexture::SetData(void *const data) const
{
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
}

CStdFramebuffer::CStdFramebuffer(const std::int32_t width, const std::int32_t height)
	: colorAttachment{width, height, InternalFormat, Format, Type}
{
	glGenFramebuffers(1, &FBO);
	colorAttachment.Bind(0);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorAttachment.GetTarget(), colorAttachment.GetTexture(), 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error{ "glCheckFramebufferStatus" };
	}

	glViewport(0, 0, width, height);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

CStdFramebuffer::~CStdFramebuffer()
{
	if (FBO)
	{
		glDeleteFramebuffers(1, &FBO);
	}
}

void CStdFramebuffer::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
}

void CStdFramebuffer::BindTexture(GLenum offset) const
{
	colorAttachment.Bind(offset);
}

void CStdFramebuffer::Unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}

/*
void CStdFramebuffer::Resize(std::int32_t newWidth, std::int32_t newHeight, CStdGLShaderProgram &copyShader, CStdRectangle &rectangle)
{
	width = newWidth;
	height = newHeight;

	CStdTexture oldColorAttachment{std::exchange(colorAttachment, CStdTexture{newWidth, newHeight, InternalFormat, Format, Type})};

	// Temp
	unsigned int tex_id = texture.Id();
	unsigned int fbo_id = fboId;

	fboId = 0;

	if (!texture.Init(newWidth, newHeight, depth))
		throw exception("Failed to resize texture");

	if (!Init())
		throw exception("Failed to resize FBO");

	// Draw old texture onto new one
	Bind();
	shader.Use();
	shader.SetInt("field", 0);
	_GL_WRAP1(glActiveTexture, GL_TEXTURE0);
	_GL_WRAP2(glBindTexture, texture.TexTarget(), tex_id);
	_GL_WRAP1(glBindVertexArray, quad.VAO);
	_GL_WRAP4(glDrawElements, GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

	// Delete old stuff
	_GL_WRAP2(glDeleteTextures, 1, &tex_id);
	_GL_WRAP2(glDeleteFramebuffers, 1, &fbo_id);
}
*/

CStdSwappableFramebuffer::CStdSwappableFramebuffer(const std::int32_t width, const std::int32_t height)
	: buffer1{width, height}, buffer2{width, height}, front{&buffer1}, back{&buffer2}
{
}

void CStdSwappableFramebuffer::Bind() const
{
	front->Bind();
}

void CStdSwappableFramebuffer::Unbind() const
{
	front->Unbind();
}

void CStdSwappableFramebuffer::SwapBuffers()
{
	std::swap(front, back);
}