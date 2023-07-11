/*	yWeb by yjogol
	internal organisation of yweb
	$Date$
	$Revision$
*/

/* define namespace */
if(typeof(Y) == "undefined")
	Y = {};
/* Class Y.yweb */
Y.yweb = new Class.create();
Object.extend(Y.yweb.prototype, {
	ver_file_prop : new Hash(),
	yweb_version:  $H({major:'0', minor:'0', patch:'0', pre:'0'}),
	yweblib_version:  $H({major:'1', minor:'0', patch:'0', pre:'0'}),
	prototype_version:  $H({major:'0', minor:'0', patch:'0', pre:'0'}),
	baselib_version:  $H({major:'1', minor:'0', patch:'0', pre:'0'}),

	initialize: function(){
		this.ver_file_get();
		split_version(this.ver_file_prop.get('version'),this.yweb_version);
		split_version(Prototype.Version,this.prototype_version);
		if(typeof(baselib_version)!="undefined")
			split_version(baselib_version,this.baselib_version);
	},
	ver_file_get: function(){
		var v_file=loadSyncURL("/Y_Version.txt");
		var lines=v_file.split("\n");
		lines.each(function(e){
			var x=e.split("=");
			if(x.length==2 && (x[0]!=""))
				this.ver_file_prop.set(x[0],x[1]);
		},this);
	},
	_require: function(min_version_str,check_version,is_msg,msg_text){
		var v=$H();
		split_version(min_version_str,v);
		if (!version_le(v, check_version)) {
			if (typeof(is_msg) != "undefined") alert(msg_text + min_version_str + " required");
			return false;
		}
		else 
			return true;
	},
	require_prototype:function(min_version_str,is_msg){
		return this._require(min_version_str,this.prototype_version,is_msg,"prototype-library version: ");
	},
	require_baselib:function(min_version_str,is_msg){
		return this._require(min_version_str,this.baselib_version,is_msg,"baselib-library version: ");
	},
	require_yweblib:function(min_version_str,is_msg){
		return this._require(min_version_str,this.yweblib_version,is_msg,"yweb-library version: ");
	},
	require:function(min_version_str,is_msg){
		return this._require(min_version_str,this.yweb_version,is_msg,"yweb version: ");
	}
});


/* main instance */
if (window == top.top_main.prim_menu) {
	var yweb = new Y.yweb();
	yweb.require_prototype("1.6");
}
else 
	if(top.top_main.prim_menu && top.top_main.prim_menu.yweb)
		var yweb = top.top_main.prim_menu.yweb;
	else { // should not happen!
		var yweb = new Y.yweb();
		yweb.require_prototype("1.6");
	}
	
/* n/m= type, menuitem, desc, file, tag, version, url, yweb_version, info_url
 * x= type, menuitem, ymenu, file, tag, version, url, yweb_version, info_url
 * u=type,site,description,url
 */

/* singleton pattern*/
if (window == top.top_main.prim_menu) {
	var ext = new Y.extension();
	ext.read_items();
}
else 
	if(top.top_main.prim_menu && top.top_main.prim_menu.ext)
		var ext = top.top_main.prim_menu.ext;
	else { // should not happen!
		var ext = new Y.extension();
		ext.read_items();
	}
