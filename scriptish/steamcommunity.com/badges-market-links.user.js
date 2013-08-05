/*jslint devel: true, browser: true, regexp: true, continue: true, plusplus: true, maxerr: 50, indent: 4, nomen: true */
/*global unsafeWindow: false */

// ==UserScript==
// @id             badges-market-links@mustached-dangerzone
// @name           Steam :: Badges :: Market Links
// @version        0.3
// @namespace      Lunatrius
// @author         Lunatrius <lunatrius@gmail.com>
// @description    Add links to the market for easier shopping!
// @match          http://steamcommunity.com/id/*/gamecards/*/
// @match          http://steamcommunity.com/id/*/inventory*
// @updateURL      https://raw.github.com/Lunatrius/mustached-dangerzone/master/scriptish/steamcommunity.com/badges-market-links.meta.js
// @downloadURL    https://raw.github.com/Lunatrius/mustached-dangerzone/master/scriptish/steamcommunity.com/badges-market-links.user.js
// @icon           https://raw.github.com/Lunatrius/mustached-dangerzone/master/scriptish/steamcommunity.com/icon.png
// @run-at         document-end
// ==/UserScript==

(function (window) {
	'use strict';

	window.__badges_market_links = {
		$: null,

		init: function () {
			if (/gamecards/i.test(location.pathname)) {
				this.jQueryLoad(this.addCardLinks);
			} else if (/inventory/i.test(location.pathname)) {
				this.addInventoryLinks();
			}

			delete this.init;

			return this;
		},

		jQueryLoad: function (callback) {
			var script, scope;

			scope = this;
			this.$$ = window.$;

			script = document.createElement('script');
			script.src = '//ajax.googleapis.com/ajax/libs/jquery/2.0.0/jquery.min.js';
			script.addEventListener('load', function () {
				return scope.jQueryLoaded();
			}, false);

			if (callback !== undefined) {
				script.addEventListener('load', function () {
					callback.call(scope);
				}, false);
			}

			document.body.appendChild(script);

			this.jQueryLoad = function (callback) {
				callback.call(scope);
			};
		},

		jQueryLoaded: function (event) {
			this.$ = window.jQuery;
			window.$ = this.$$;

			delete this.$$;
			delete this.jQueryLoaded;
		},

		addCardLinks: function () {
			var $, header, match, text, html, name;

			$ = this.$;
			header = $('.profile_small_header_text');
			text = header.find('.profile_small_header_location').last().text();

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
					.attr('href', '/market/search?q=' + encodeURIComponent(text))
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
		},

		addInventoryLinks: function () {
			var populateActionsOld = window.PopulateActions;

			window.PopulateActions = function (elActions, rgActions, item) {
				populateActionsOld(elActions, rgActions, item);

				if (/Trading Card/i.test(item.type)) {
					var elAction = new window.Element('a', {
						'class': 'item_action',
						'href': '/market/listings/753/' + encodeURIComponent(item.market_hash_name),
						'target': 'steamcommunitymarket'
					});
					elAction.update('View on market');

					elActions.appendChild(elAction);
				}
			};
		}
	}.init();
}(unsafeWindow));
