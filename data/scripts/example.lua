-- This is a comment

ScriptProperties = {
    duplicateEntityName = { type = "string", value = "" }
}

function Update(delta)
    entity = GetThisEntity()

    transform = entity:GetTransformComponent()

    local time = Atlas.Clock.Get()

    local offset = Glm.Vec3(0.0, math.sin(time), 0.0) + Glm.Vec3(0.0, 5.0, 0.0)
    local matrix = Glm.Translate(offset)

    Atlas.Log.Warning(tostring(offset.y))

    transform:Set(matrix)
end