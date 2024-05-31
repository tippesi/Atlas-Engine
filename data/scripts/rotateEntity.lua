-- This is a comment

ScriptProperties = {
    rotationAxis = { type = "integer", value = 0 },
    rotationSpeed = { type = "double", value = 1.0 }
}

totalRotation = 0.0

function Update(delta)
    
    local entity = GetThisEntity()
    local scene = GetThisScene()

    transform = entity:GetTransformComponent()

    if transform == nil then
        return
    end

    local decomp = transform:Decompose()

    local offset = Atlas.Clock.GetDelta() * ScriptProperties.rotationSpeed.value
    totalRotation = totalRotation + offset

    local rotationVec = Glm.Vec3(0.0)
    if ScriptProperties.rotationAxis.value == 0 then
        rotationVec.x = 1.0
    end
    if ScriptProperties.rotationAxis.value == 1 then
        rotationVec.y = 1.0
    end
    if ScriptProperties.rotationAxis.value == 2 then
        rotationVec.z = 1.0
    end

    local updateMatrix = Glm.Rotate(transform.matrix, offset, rotationVec)

    local matrix = decomp:Compose()
    transform:Set(updateMatrix)

end