imports.gi.versions.Gdk = '3.0';
imports.gi.versions.Gtk = '3.0';
imports.searchPath.unshift('/usr/local/share/clapper');

const ClapperSrc = imports.clapper_src;

var { App } = ClapperSrc.app;
var { Interface } = ClapperSrc.interface;
var { Player } = ClapperSrc.player;

imports.searchPath.shift();
