-- This is a comment

ScriptProperties = {
    spawnEntityName = { type = "string", value = "" },
    spawnRate = { type = "double", value = 1.0 }
}

lastSpawn = 0.0

function Update(delta)

    -- Only update the player rotation if there is input
    local playerInput = (Atlas.KeyboardMap.IsKeyPressed(Atlas.Keycode.KeyW, true) or 
        Atlas.KeyboardMap.IsKeyPressed(Atlas.Keycode.KeyA, true) or
        Atlas.KeyboardMap.IsKeyPressed(Atlas.Keycode.KeyS, true) or
        Atlas.KeyboardMap.IsKeyPressed(Atlas.Keycode.KeyD, true))

    if playerInput ~= true then
        return
    end
    
    local entity = GetThisEntity()
    local scene = GetThisScene()

    local camera = entity:GetCameraComponent()
    local transform = entity:GetTransformComponent()
    local player = entity:GetPlayerComponent()

    local rotationMatrix = Glm.Rotate(Glm.Mat4(1.0), camera.rotation.x, Glm.Vec3(0.0, 1.0, 0.0))
    local recomposed = Atlas.MatrixDecomposition(rotationMatrix)

    player:SetRotation(Glm.Quat(recomposed.rotation))

end