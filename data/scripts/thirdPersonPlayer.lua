-- This is a comment

ScriptProperties = {
    spawnEntityName = { type = "string", value = "" },
    spawnRate = { type = "double", value = 1.0 }
}

lastSpawn = 0.0

function Update(delta)
    
    local entity = GetThisEntity()
    local scene = GetThisScene()

    local camera = entity:GetCameraComponent()
    local transform = entity:GetTransformComponent()
    local player = entity:GetPlayerComponent()

    local rotationMatrix = Glm.Rotate(Glm.Mat4(1.0), camera.rotation.x, Glm.Vec3(0.0, 1.0, 0.0))
    local recomposed = Atlas.MatrixDecomposition(rotationMatrix)

    player:SetRotation(Glm.Quat(recomposed.rotation))

end