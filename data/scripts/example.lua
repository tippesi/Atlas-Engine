-- This is a comment

function update (delta)
    entity = get_this_entity()

    transform = entity:get_transform_component()

    local time = Atlas.Clock.get()

    local offset = Glm.Vec3(0.0, math.sin(time), 0.0) + Glm.Vec3(0.0, 5.0, 0.0)
    local matrix = Glm.translate(offset)

    Atlas.Log.warning(tostring(offset.y))

    transform:set(matrix)
end