//
//#include <string>
//#include <fstream>
//#include <sstream>
//#include <iostream>
//
//class Shader {
//public:
//    GLuint shaderProgram;
//    GLuint vertexShader;
//    GLuint fragmentShader;
//
//    Shader(const std::string& vertexPath, const std::string& fragmentPath) {
//        vertexShader = loadShader(vertexPath, GL_VERTEX_SHADER);
//        fragmentShader = loadShader(fragmentPath, GL_FRAGMENT_SHADER);
//
//        shaderProgram = glCreateProgram();
//        glAttachShader(shaderProgram, vertexShader);
//        glAttachShader(shaderProgram, fragmentShader);
//        glLinkProgram(shaderProgram);
//
//        GLint success;
//        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
//        if (!success) {
//            char infoLog[512];
//            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
//            std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
//        }
//
//        glDeleteShader(vertexShader);
//        glDeleteShader(fragmentShader);
//    }
//
//    ~Shader() {
//        glDeleteProgram(shaderProgram);
//    }
//
//    void use() {
//        glUseProgram(shaderProgram);
//    }
//
//private:
//    GLuint loadShader(const std::string& path, GLenum shaderType) {
//        std::ifstream shaderFile(path);
//        if (!shaderFile.is_open()) {
//            std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << path << std::endl;
//            return 0;
//        }
//
//        std::stringstream shaderStream;
//        shaderStream << shaderFile.rdbuf();
//        shaderFile.close();
//
//        std::string shaderCode = shaderStream.str();
//        const char* shaderCodeCStr = shaderCode.c_str();
//
//        GLuint shader = glCreateShader(shaderType);
//        glShaderSource(shader, 1, &shaderCodeCStr, NULL);
//        glCompileShader(shader);
//
//        GLint success;
//        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
//        if (!success) {
//            char infoLog[512];
//            glGetShaderInfoLog(shader, 512, NULL, infoLog);
//            std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
//        }
//
//        return shader;
//    }
//};
