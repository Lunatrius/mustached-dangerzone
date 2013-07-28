/*jslint devel: true, browser: true, regexp: true, continue: true, plusplus: true, maxerr: 50, indent: 4 */

// ==UserScript==
// @id             com.steamcommunity.badges-market-links@com.github.lunatrius.mustached-dangerzone
// @name           Steam :: Badges :: Market Links
// @version        0.1.2
// @namespace      Lunatrius
// @author         Lunatrius <lunatrius@gmail.com>
// @description    Add links to the market for easier shopping!
// @match          http://steamcommunity.com/id/*/gamecards/*/
// @updateURL      https://raw.github.com/Lunatrius/mustached-dangerzone/master/scriptish/steamcommunity.com/badges-market-links.user.js
// @icon           https://raw.github.com/Lunatrius/mustached-dangerzone/master/scriptish/steamcommunity.com/icon.png
//  icon source    http://nickts.deviantart.com/art/Steam-Stripe-Icon-163889127
// @run-at         document-end
// ==/UserScript==

(function () {
	"use strict";

	var $, script, jQueryLoaded, addLinks;

	jQueryLoaded = function (event) {
		$ = window.$ || window.wrappedJSObject.$;

		if ($ !== undefined && $ !== null) {
			addLinks();
		}
	};

	addLinks = function (event) {
		var game, text, html, name;

		game = $('.profile_small_header_location').last().text().trim();

		$('.badge_card_set_text').each(function (id, node) {
			node = $(node);
			text = node.text();
			html = node.html();

			if (!/\d of \d/i.test(text) && !/None of your friends have this card./i.test(text)) {
				name = node.clone().children().remove().end().text().trim();
				html = html.replace(name, '<a href="/market/search?q=' + encodeURIComponent(game + ' ' + name) + '" target="steamcommunitymarket">' + name + '</a>');
				node.html(html);
			}
		});
	};

	// add jQuery
	script = document.createElement("script");
	script.src = "//ajax.googleapis.com/ajax/libs/jquery/2.0.0/jquery.min.js";
	script.addEventListener('load', jQueryLoaded, false);
	document.body.appendChild(script);
}());
