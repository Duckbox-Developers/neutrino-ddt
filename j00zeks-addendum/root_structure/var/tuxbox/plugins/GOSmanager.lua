-- j00zTrino manager
-- @j00zek 2016.04.08

-- inits
local n = neutrino()

--functions
function CancelKey(a)
	return MENU_RETURN.EXIT_REPAINT
end 

function CECconfig(a)
	n:runScript("/var/tuxbox/plugins/GOSmanager/CECconfig.lua")
end 

function VFDconfig(a)
	n:runScript("/var/tuxbox/plugins/GOSmanager/VFDconfig.lua")
end 

function GOSconfig(a)
	n:runScript("/var/tuxbox/plugins/GOSmanager/GOSconfig.lua")
end 

function IPTVnPlayer(a)
	--n:runScript("/var/tuxbox/plugins/GOSmanager/IPTVnPlayer.lua")
	messagebox.exec{title="IPTVnPlayer", text="Będzie dostępny, jak będzie, nie pytać kiedy :P", buttons={ "ok" }, has_shadow=true } 
end 

-- menu
function jMenu()
	local m = menu.new{name="GOSmanager @j00zek", has_shadow=true}
	m:addKey{directkey = RC["home"], id = "home", action = "CancelKey"}
	m:addItem{type = "back"}
	m:addItem{type="separatorline"}
	m:addItem{type="forwarder", name="CEC settings", action="CECconfig", icon="rot", directkey=RC["red"]}
	m:addItem{type="forwarder", name="VFD icons settings", action="VFDconfig", icon="rot", directkey=RC["red"]}
	m:addItem{type="forwarder", name="Graterlia settings", action="GOSconfig", icon="gelb", directkey=RC["gelb"]}
	m:addItem{type="forwarder", name="IPTVnPlayer", action="IPTVnPlayer", icon="blau", directkey=RC["blau"]}
	m:exec()
end 

jMenu()