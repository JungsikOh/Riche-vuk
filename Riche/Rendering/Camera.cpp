#include "Camera.h"

void Camera::Initialize(CameraParameters const& params)
{
	m_Position = params.position;
	m_LookVector = params.lootAt;
	m_NearPlane = params.nearPlane;
	m_FarPlane = params.farPlane;
	m_AspectRatio = params.aspectRatio;
	m_Fov = params.fov;

	m_SpeedFactor = params.speed;
	m_Sensitivity = params.sensitivity;

	SetViewMat(m_Position, m_LookVector, m_UpVector);
	SetProjMat(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
}

void Camera::Update()
{
	m_LookVector = glm::normalize(m_LookVector);
	m_RightVector = glm::normalize(glm::cross(m_LookVector, m_UpVector));
	m_UpVector = glm::cross(m_RightVector, m_LookVector);
	SetViewMat(m_Position, m_LookVector, m_UpVector);
	SetProjMat(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
}

void Camera::Tick(float dt, float dx, float dy, int key, int button)
{
	Update();
}

void Camera::OnKeyInput(float dt, int key)
{
	if (key == GLFW_KEY_W)
	{
		MoveFoward(dt);
	}
	if (key == GLFW_KEY_S)
	{
		MoveFoward(-dt);
	}
	if (key == GLFW_KEY_A)
	{
		MoveRight(-dt);
	}
	if (key == GLFW_KEY_D)
	{
		MoveRight(dt);
	}
	if (key == GLFW_KEY_Q)
	{
		MoveUp(dt);
	}
	if (key == GLFW_KEY_E)
	{
		MoveUp(-dt);
	}
	Update();
}

void Camera::OnMouseInput(float dx, float dy)
{
	RotatePitch((int64_t)dy);
	RotateYaw((int64_t)dx);

	Update();
}

void Camera::SetPosition(glm::vec3 const& pos)
{
	m_Position = pos;
	Update();
}

void Camera::SetNearAndFar(float n, float f)
{
	m_NearPlane = n;
	m_FarPlane = f;
	Update();
}

void Camera::SetAspectRatio(float ar)
{
	m_AspectRatio = ar;
	Update();
}

void Camera::SetFov(float _fov)
{
	m_Fov = _fov;
	Update();
}

void Camera::SetProjMat(float fov, float aspect, float zn, float zf)
{
	m_ProjMat = glm::perspective(glm::radians(fov), aspect, zn, zf);
	m_ProjMat[1][1] *= -1;
}

void Camera::SetViewMat(glm::vec3 pos, glm::vec3 lookAt, glm::vec3 up)
{
	m_ViewMat = glm::lookAt(pos, pos + lookAt, up);
}

void Camera::MoveFoward(float dt)
{
	glm::vec3 oldPos = m_Position;
	glm::vec3 newPos = oldPos + dt * m_SpeedFactor * m_LookVector;
	SetPosition(newPos);
}

void Camera::MoveRight(float dt)
{
	glm::vec3 oldPos = m_Position;
	glm::vec3 newPos = oldPos + dt * m_SpeedFactor * m_RightVector;
	SetPosition(newPos);
}

void Camera::MoveUp(float dt)
{
	glm::vec3 oldPos = m_Position;
	glm::vec3 newPos = oldPos + dt * m_SpeedFactor * m_UpVector;
	SetPosition(newPos);
}

void Camera::RotatePitch(int64_t dy)
{
	float angle = m_Sensitivity * glm::radians(static_cast<float>(dy));
	glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), angle, m_RightVector);

	m_UpVector = glm::normalize(glm::vec3(rotate * glm::vec4(m_UpVector, 0.0f)));
	m_LookVector = glm::normalize(glm::vec3(rotate * glm::vec4(m_LookVector, 0.0f)));
}

void Camera::RotateYaw(int64_t dx)
{
	float angle = m_Sensitivity * glm::radians(static_cast<float>(dx));
	glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));

	m_RightVector = glm::normalize(glm::vec3(rotate * glm::vec4(m_RightVector, 0.0f)));
	m_LookVector = glm::normalize(glm::vec3(rotate * glm::vec4(m_LookVector, 0.0f)));
	m_UpVector = glm::normalize(glm::cross(m_RightVector, m_LookVector));
}
