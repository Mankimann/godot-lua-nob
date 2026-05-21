local M = {}

function M.add(a, b)
    return a + b
end

function M.length2(v)
    return v.x * v.x + v.y * v.y
end

return M
