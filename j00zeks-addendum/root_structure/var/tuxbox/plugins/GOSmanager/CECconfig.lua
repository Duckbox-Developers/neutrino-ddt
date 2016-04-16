local n = neutrino()
local w = cwindow.new{x=50, y=50, dx=400, dy=200, name="CEC settings", icon="info", btnRed="TBC"}
w:paint()

repeat
	msg, data = n:GetInput(500)
	if (msg == RC['red']) then
		print("Będzie później")
	end
until msg == RC['home']
w:hide()
