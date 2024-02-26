-- This is a comment

ScriptProperties = {
    spawnEntityName = { type = "string", value = "" },
    spawnRate = { type = "double", value = 1.0 }
}

lastSpawn = 0.0

function Update(delta)
    
    local entity = GetThisEntity()
    local scene = GetThisScene()
    local spawnEntity = scene:GetEntityByName(ScriptProperties.spawnEntityName.value)

    if not spawnEntity:IsValid() then
        return
    end

    transform = entity:GetTransformComponent()

    spawnTransform = spawnEntity:GetTransformComponent()
    spawnRigidBody = spawnEntity:GetRigidBodyComponent()

    if spawnTransform == nil or spawnRigidBody == nil or transform == nil then
        return
    end

    local time = Atlas.Clock.Get()
    local spawnFraction = 1.0 / ScriptProperties.spawnRate.value

    if time - lastSpawn > spawnFraction then
        local decomp = Atlas.MatrixDecomposition(transform.globalMatrix)

        local spawnCenter = decomp.translation

        local deg = math.random(0.0, 1.0) * 2.0 * 3.14159
        local dist = math.random(0.0, 1.0) + math.random(0.0, 1.0)

        if dist > 1.0 then 
            dist = 2.0 - dist 
        end

        local dir = Glm.Vec3(math.cos(deg), 0.0, math.sin(deg)) * dist * 5.0

        local spawnPoint = dir + spawnCenter
        local matrix = Glm.Translate(spawnPoint)

        local newEntity = scene:DuplicateEntity(spawnEntity)

        local newTransform = newEntity:GetTransformComponent()
        local newRigidBody = newEntity:GetRigidBodyComponent()

        newRigidBody:SetLinearVelocity(dir)
        newTransform:Set(matrix)

        lastSpawn = time

        Atlas.Log.Warning("New spawn")
    end
end