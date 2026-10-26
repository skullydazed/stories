(() =>
{
	"use strict";

	const root = document.documentElement;
	let preference = "system";

	try
	{
		const saved = localStorage.getItem("stories-theme");
		if (saved === "light" || saved === "dark")
			preference = saved;
	}
	catch
	{
		// Storage may be unavailable; switching still works for the current page.
	}

	// Apply a saved mode before loading styles to avoid flashing the system mode.
	if (preference !== "system")
		root.dataset.theme = preference;

	document.addEventListener("DOMContentLoaded", () =>
	{
		const switcher = document.getElementById("theme-switcher");
		const control = document.getElementById("theme-control");
		if (!switcher || !control)
			return;

		const system = window.matchMedia("(prefers-color-scheme: dark)");

		function updateControl()
		{
			const label = preference[0].toUpperCase() + preference.slice(1);
			const dark = preference === "system" ? system.matches : preference === "dark";
			switcher.setAttribute("aria-checked", String(dark));
			switcher.title = "Color scheme: " + label + (preference === "system" ? (dark ? " (Dark)" : " (Light)") : "");
		}

		updateControl();
		system.addEventListener("change", () =>
		{
			if (preference === "system")
				updateControl();
		});
		control.hidden = false;
		switcher.addEventListener("click", () =>
		{
			const dark = preference === "system" ? system.matches : preference === "dark";
			preference = dark ? "light" : "dark";
			root.dataset.theme = preference;
			updateControl();

			try
			{
				localStorage.setItem("stories-theme", preference);
			}
			catch
			{
				// Keep the selected mode even when it cannot be remembered.
			}
		});
	});
})();
