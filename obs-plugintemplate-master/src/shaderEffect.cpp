//#include <obs-module.h>
//#include <graphics/graphics.h>
//#include "shader.cpp"
//
//class ShaderEffect {
//public:
//    Shader* shader;
//    obs_source_t* source;
//
//    ShaderEffect(const std::string& vertexPath, const std::string& fragmentPath, obs_source_t* source) {
//        this->source = source;
//        shader = new Shader(vertexPath, fragmentPath);
//    }
//
//    ~ShaderEffect() {
//        delete shader;
//    }
//
//    void apply() {
//        if (!source) {
//            std::cerr << "Error: Source is not initialized" << std::endl;
//            return;
//        }
//
//        shader->use();
//
//        GLuint textureId = getTextureIdFromSource(source);
//        if (textureId != 0) {
//            glBindTexture(GL_TEXTURE_2D, textureId);
//            glUniform1i(glGetUniformLocation(shader->shaderProgram, "tex0"), 0); // Bind texture unit 0
//            glBindTexture(GL_TEXTURE_2D, 0);
//        }
//    }
//
//private:
//    GLuint getTextureIdFromSource(obs_source_t* source) {
//        // Acceder a la textura de la fuente multimedia
//        obs_source_frame_t* frame = obs_source_get_frame(source);
//        if (!frame) {
//            std::cerr << "Error: Failed to get frame from source" << std::endl;
//            return 0;
//        }
//
//        GLuint textureId = frame->texture;
//        obs_source_release_frame(source);
//        return textureId;
//    }
//};
