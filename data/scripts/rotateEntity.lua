-- This is a comment

ScriptProperties = {
    rotationAxis = { type = "integer", value = 0 },
    rotationSpeed = { type = "double", value = 1.0 }
}

function Update(delta)
    
    local entity = GetThisEntity()
    local scene = GetThisScene()

    Atlas.Log.Warning(tostring(entity))

    transform = entity:GetTransformComponent()

    if transform == nil then
        return
    end

    local decomp = Atlas.MatrixDecomposition(transform.globalMatrix)

    local offset = Atlas.Clock.GetDelta() * ScriptProperties.rotationSpeed.value

    if ScriptProperties.rotationAxis.value == 0 then
        decomp.rotation.x = decomp.rotation.x + offset;
    end
    if ScriptProperties.rotationAxis.value == 1 then
        decomp.rotation.y = decomp.rotation.y + offset;
        if decomp.rotation.y > 3.14 / 2.0 then
            decomp.rotation.y = -decomp.rotation.y
        end
    end
    if ScriptProperties.rotationAxis.value == 2 then
        decomp.rotation.z = decomp.rotation.z + offset;
    end

    local matrix = decomp:Compose()
    transform:Set(matrix)

end