-- This is a comment

ScriptProperties = {
    animationLength = { type = "double", value = 30.0 },
    animationActive = { type = "boolean", value = false }
}

animationTime = 0.0

function Update(delta)
    
    local entity = GetThisEntity()
    local scene = GetThisScene()

    if scene:HasMainCamera() ~= true or ScriptProperties.animationActive.value ~= true then
        return
    end

    local camera = scene:GetMainCamera()
    local hierarchy = entity:GetHierarchyComponent()

    local children = hierarchy:GetChildren()

    if children == nil then
        return
    end

    if #children <= 1 then
        return
    end

    animationTime = animationTime + Atlas.Clock.GetDelta()

    local animationProgress = animationTime / ScriptProperties.animationLength.value
    if animationProgress >= 1.0 then
        return
    end

    local childProgress = animationProgress * (#children - 1.0)

    local lowerChildIndex = math.floor(childProgress)
    local upperChildIndex = math.ceil(childProgress)
    local childFraction = childProgress - lowerChildIndex

    local lowerChildEntity = children[lowerChildIndex + 1]
    local upperChildEntity = children[upperChildIndex + 1]

    local lowerChildCamera = lowerChildEntity:GetCameraComponent()
    local upperChildCamera = upperChildEntity:GetCameraComponent()

    camera.location = Glm.Mix(lowerChildCamera:GetLocation(), upperChildCamera:GetLocation(), childFraction)
    camera.rotation = Glm.Mix(lowerChildCamera.rotation, upperChildCamera.rotation, childFraction)

end