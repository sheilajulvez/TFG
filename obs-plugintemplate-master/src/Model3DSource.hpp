// Model3DSource.hpp

#ifndef MODEL3DSOURCE_HPP
#define MODEL3DSOURCE_HPP

#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <vector>

class Model3DSource : public QOpenGLFunctions_4_5_Core {
public:
	Model3DSource(const QString &path);
	~Model3DSource();

	void render(int width, int height);

private:
	void loadModel(const QString &path);

	QOpenGLBuffer vbo;
	QOpenGLVertexArrayObject vao;
	QOpenGLShaderProgram program;
	std::vector<float> vertices;
	QMatrix4x4 projectionMatrix;
};

#endif // MODEL3DSOURCE_HPP
