#pragma once
struct CameraParameters
{
    float aspectRatio;
    float nearPlane;
    float farPlane;
    float fov;
    float speed;
    float sensitivity;
    glm::vec3 position;
    glm::vec3 lootAt;
};

class Camera
{
    glm::vec3 m_Position;
    glm::vec3 m_RightVector = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 m_UpVector = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_LookVector = glm::vec3(0.0f, 0.0f, 1.0f);

    glm::mat4 m_ViewMat = glm::mat4(1.0f);
    glm::mat4 m_ProjMat = glm::mat4(1.0f);

    float m_AspectRatio;
    float m_Fov;
    float m_NearPlane;
    float m_FarPlane;

    float m_SpeedFactor = 1.0f;
    float m_Sensitivity = 0.3f;

public:
    Camera() = default;
    ~Camera() = default;

    void Initialize(CameraParameters const& params);
    void Update();
    void Tick(float dt, float dx = 0, float dy = 0, int key = -1, int button = -1);
    void OnKeyInput(float dt, int key);
    void OnMouseInput(float dx, float dy);

    void SetPosition(glm::vec3 const& pos);
    void SetNearAndFar(float n, float f);
    void SetAspectRatio(float ar);
    void SetFov(float _fov);
    void SetProjMat(float fov, float aspect, float zn, float zf);
    void SetViewMat(glm::vec3 pos, glm::vec3 lookAt, glm::vec3 up);

    void MoveFoward(float dt);
    void MoveRight(float dt);
    void MoveUp(float dt);
    void RotatePitch(int64_t dy);
    void RotateYaw(int64_t dx);

    glm::vec3 Position() const
    {
        return m_Position;
    }
    glm::vec3 Up() const
    {
        return m_UpVector;
    }
    glm::vec3 Right() const
    {
        return m_RightVector;
    }
    glm::vec3 Forward() const
    {
        return m_LookVector;
    }

    float Near() const
    {
        return m_NearPlane;
    }
    float Far() const
    {
        return m_FarPlane;
    }
    float Fov() const
    {
        return m_Fov;
    }
    float AspectRatio() const
    {
        return m_AspectRatio;
    }

    glm::mat4 View()
    {
        return m_ViewMat;
    }
    glm::mat4 Proj()
    {
        return m_ProjMat;
    }
    glm::mat4 ViewProj()
    {
        return m_ProjMat * m_ViewMat;
    }
};

