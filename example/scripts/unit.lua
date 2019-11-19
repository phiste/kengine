-- Simply modify component

local transform = self:getTransformComponent()
local pos = transform.boundingBox.position
pos.x = pos.x + 1

