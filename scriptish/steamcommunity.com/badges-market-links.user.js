/*jslint devel: true, browser: true, regexp: true, continue: true, plusplus: true, maxerr: 50, indent: 4 */
/*global unsafeWindow: false */

// ==UserScript==
// @id             com.steamcommunity.badges-market-links@com.github.lunatrius.mustached-dangerzone
// @name           Steam :: Badges :: Market Links
// @version        0.2
// @namespace      Lunatrius
// @author         Lunatrius <lunatrius@gmail.com>
// @description    Add links to the market for easier shopping!
// @match          http://steamcommunity.com/id/*/gamecards/*/
// @match          http://steamcommunity.com/id/*/inventory/*
// @updateURL      https://raw.github.com/Lunatrius/mustached-dangerzone/master/scriptish/steamcommunity.com/badges-market-links.user.js
// @icon           https://raw.github.com/Lunatrius/mustached-dangerzone/master/scriptish/steamcommunity.com/icon.png
//  icon source    http://nickts.deviantart.com/art/Steam-Stripe-Icon-163889127
// @run-at         document-end
// ==/UserScript==

(function () {
	// 'use strict';

	if (/gamecards/i.test(location.pathname)) {
		var $, addLinks, jQueryLoaded, script;

		jQueryLoaded = function (event) {
			unsafeWindow.$$ = unsafeWindow.$;
			unsafeWindow.$ = $;
			$ = unsafeWindow.$$;

			if ($ !== undefined && $ !== null) {
				addLinks();
			}
		};

		addLinks = function (event) {
			console.log('foobar');
			var header, match, text, html, name;

			header = $('.profile_small_header_text');

			header.append(' ');
			header.append(
				$('<span></span>')
					.addClass('profile_small_header_arrow')
					.html('&raquo;')
			);
			header.append(' ');
			header.append(
				$('<a></a>')
					.addClass('whiteLink')
					.attr('href', '#')
					.append(
						$('<span></span>')
							.text('Market')
					)
			);

			match = location.pathname.match(/\/gamecards\/(\d+)\//i);

			if (match !== null) {
				$('.badge_card_set_text').each(function (id, node) {
					node = $(node);
					text = node.text();
					html = node.html();

					if (!/\d of \d/i.test(text) && !/None of your friends have this card./i.test(text)) {
						name = node.clone().children().remove().end().text().trim();
						html = html.replace(name, '<a href="/market/listings/753/' + encodeURIComponent(match[1] + '-' + name) + '" target="steamcommunitymarket">' + name + '</a>');
						node.html(html);
					}
				});
			}
		};

		// add jQuery
		$ = unsafeWindow.$;
		script = document.createElement('script');
		script.src = '//ajax.googleapis.com/ajax/libs/jquery/2.0.0/jquery.min.js';
		script.addEventListener('load', jQueryLoaded, false);
		document.body.appendChild(script);
	}

	if (/inventory/i.test(location.pathname)) {
		var populateActionsOld = unsafeWindow.PopulateActions;

		unsafeWindow.PopulateActions = function (elActions, rgActions, item) {
			populateActionsOld(elActions, rgActions, item);

			if (/Trading Card/i.test(item.type)) {
				var elAction = new unsafeWindow.Element('a', {
					'class': 'item_action',
					'href': '/market/listings/753/' + encodeURIComponent(item.market_hash_name),
					'target': 'steamcommunitymarket'
				});
				elAction.update('View on market');

				elActions.appendChild(elAction);
			}
		};
	}
}());
