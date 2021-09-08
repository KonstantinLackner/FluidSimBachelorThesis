#pragma once
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <string_view>
#include <unordered_map>

// shader
class CStdShader
{
public:
	class Exception : public std::runtime_error
	{
	public:
		using runtime_error::runtime_error;
	};

	enum class Type : uint8_t
	{
		Vertex,
		TesselationControl,
		TesselationEvaluation,
		Geometry,
		Fragment,
		Compute
	};

public:
	CStdShader() = default;
	explicit CStdShader(Type type, const std::string& source) : type{ type }, source{ source } {}
	CStdShader(const CStdShader&) = delete;

	virtual ~CStdShader() { Clear(); }

	void SetMacro(const std::string& key, const std::string& value);
	void UnsetMacro(const std::string& key);
	void SetSource(const std::string& source);
	void AddInclude(const std::string& source);
	void SetType(Type type);

	virtual void Compile() = 0;
	virtual void Clear();

	std::string GetSource() const { return source; }
	virtual int64_t GetHandle() const = 0;
	std::unordered_map<std::string, std::string> GetMacros() const { return macros; }
	std::string GetErrorMessage() const { return errorMessage; }
	virtual Type GetType() const { return type; }

protected:
	Type type;
	std::string source;
	std::vector<std::string> includes;
	std::unordered_map<std::string, std::string> macros;
	std::string errorMessage;
};

class CStdGLShader : public CStdShader
{
public:
	using CStdShader::CStdShader;

	void Compile() override;
	void Clear() override;

	virtual int64_t GetHandle() const override { return shader; }

protected:
	virtual void PrepareSource();

protected:
	GLuint shader = 0;
};

class CStdShaderProgram
{
public:
	class Exception : public std::runtime_error
	{
	public:
		using runtime_error::runtime_error;
	};

public:
	CStdShaderProgram() = default;
	CStdShaderProgram(const CStdShaderProgram&) = delete;
	virtual ~CStdShaderProgram() { Clear(); }

	virtual explicit operator bool() const = 0;

	bool AddShader(CStdShader* shader);

	virtual void Link() = 0;
	void Select();
	static void Deselect();
	virtual void Clear();

	virtual void EnsureProgram() = 0;
	virtual int64_t GetProgram() const = 0;

	std::vector<CStdShader*> GetPendingShaders() const { return shaders; }
	static CStdShaderProgram* GetCurrentShaderProgram();

protected:
	virtual bool AddShaderInt(CStdShader* shader) = 0;
	virtual void OnSelect() = 0;
	virtual void OnDeselect() = 0;

protected:
	std::vector<CStdShader*> shaders;
	std::string errorMessage;
	static CStdShaderProgram* currentShaderProgram;
};

class CStdGLShaderProgram : public CStdShaderProgram
{
public:
	using CStdShaderProgram::CStdShaderProgram;

	explicit operator bool() const override { return /*glIsProgram(*/shaderProgram/*)*/; }

	void Link() override;
	void Clear() override;

	void EnsureProgram() override;

	template<typename Func, typename... Args> bool SetAttribute(const std::string& key, Func function, Args... args)
	{
		return SetAttribute(key, &CStdGLShaderProgram::attributeLocations, glGetAttribLocation, function, args...);
	}

	template<typename Func, typename... Args> bool SetUniform(const std::string& key, Func function, Args... args)
	{
		return SetAttribute(key, &CStdGLShaderProgram::uniformLocations, glGetUniformLocation, function, args...);
	}

	bool SetUniform(const std::string& key, float value) { return SetUniform(key, glUniform1f, value); }
	bool SetUniform(const std::string& key, const glm::vec2& value);
	bool SetUniform(const std::string& key, const glm::vec3& value);
	bool SetUniform(const std::string& key, const glm::vec4& value);
	bool SetUniform(const std::string& key, const glm::mat4& value);

	void EnterGroup(const std::string& name) { group.assign(name).append("."); }
	void LeaveGroup() { group.clear(); }

	void SetObjectLabel(std::string_view label);

	virtual int64_t GetProgram() const override { return shaderProgram; }

protected:
	bool AddShaderInt(CStdShader* shader) override;
	void OnSelect() override;
	void OnDeselect() override;

	using Locations = std::unordered_map<std::string, GLint>;
	template<typename MapFunc, typename SetFunc, typename... Args> bool SetAttribute(const std::string& key, Locations CStdGLShaderProgram::* locationPointer, MapFunc mapFunction, SetFunc setFunction, Args... args)
	{
		assert(shaderProgram);

		std::string realKey{ group };
		realKey.append(key);

		GLint location;
		Locations& locations{ this->*locationPointer };
		if (auto it = locations.find(realKey); it != locations.end())
		{
			location = it->second;
			assert(location != -1);
		}
		else
		{
			location = mapFunction(shaderProgram, realKey.c_str());
			if (location == -1)
			{
				return false;
			}

			locations.emplace(realKey, location);
		}
		setFunction(location, args...);

		return true;
	}

protected:
	GLuint shaderProgram{ 0 };

	Locations attributeLocations;
	Locations uniformLocations;

	std::string group;
};

template<typename Class>
class CStdVAOObject
{
public:
	CStdVAOObject() = default;
	virtual ~CStdVAOObject()
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(VBO.size(), VBO.data());
	}

public:
	void Bind() const
	{
		glBindVertexArray(VAO);
	}
	void Draw() const
	{
		glDrawElements(Class::PrimitiveType, elementCount, GL_UNSIGNED_INT, nullptr);
	}

protected:
	void Init()
	{
		assert(!VAO);
		glGenVertexArrays(1, &VAO);
		glGenBuffers(VBO.size(), VBO.data());

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO[VBO.size() - 1]);

		std::vector<GLfloat> vertices;
		std::vector<GLuint> elements;
		std::vector<GLfloat> normals;
		std::vector<GLfloat> textureCoordinates;
		GenerateGeometry(vertices, elements, normals, textureCoordinates);
		elementCount = elements.size();

		constexpr std::size_t dimensions{Class::Dimensions};

		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementCount * sizeof(GLuint), elements.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, dimensions, GL_FLOAT, GL_FALSE, dimensions * sizeof(GLfloat), nullptr);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(1, dimensions, GL_FLOAT, GL_FALSE, dimensions * sizeof(GLfloat), nullptr);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
		glBufferData(GL_ARRAY_BUFFER, textureCoordinates.size() * sizeof(GLfloat), textureCoordinates.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
		glEnableVertexAttribArray(2);

		glBindVertexArray(GL_NONE);
		glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_NONE);
	}

	virtual void GenerateGeometry(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices, std::vector<GLfloat> &normals, std::vector<GLfloat> &textureCoordinates) = 0;
private:
	std::size_t elementCount{0};
	GLuint VAO{GL_NONE};
	std::array<GLuint, 4> VBO{GL_NONE};
};

class CStdTexture
{
public:
	CStdTexture(std::int32_t width, std::int32_t height, GLenum internalFormat, GLenum format, GLenum type, void *const data = nullptr);
	CStdTexture(const CStdTexture &) = delete;
	CStdTexture(CStdTexture&& other) = default;
	~CStdTexture();

	CStdTexture& operator=(const CStdTexture& other) = delete;
	CStdTexture& operator=(CStdTexture &&other) = default;

public:
	void Bind(GLenum offset) const;
	void SetData(void *const data) const;
	virtual GLenum GetTarget() const { return GL_TEXTURE_2D; }

	GLuint GetTexture() const { return texture; }

protected:
	GLuint texture{GL_NONE};
	std::int32_t width;
	std::int32_t height;
	GLenum internalFormat;
	GLenum format;
	GLenum type;
};

static_assert(std::is_move_constructible_v<CStdTexture>);

class CStdFramebuffer
{
public:
	CStdFramebuffer(std::int32_t width, std::int32_t height);
	~CStdFramebuffer();

public:
	void Bind() const;
	void BindTexture(GLenum offset) const;
	void Unbind() const;
	const CStdTexture &GetTexture() const { return colorAttachment; }

private:
	static constexpr inline auto InternalFormat = GL_RG16F;
	static constexpr inline auto Format = GL_RG;
	static constexpr inline auto Type = GL_FLOAT;

	CStdTexture colorAttachment;
	GLuint FBO;
};

class CStdSwappableFramebuffer
{
public:
	CStdSwappableFramebuffer(std::int32_t width, std::int32_t height);

public:
	void Bind() const;
	void Unbind() const;
	void SwapBuffers();

	const CStdFramebuffer &GetFront() const { return *front; }
	const CStdFramebuffer &GetBack() const { return *back; }

private:
	std::array<CStdFramebuffer, 2> buffers;
	CStdFramebuffer *front;
	CStdFramebuffer *back;
};
