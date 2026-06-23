<tvh_include>inc/config_contents</tvh_include>

---

<tvh_include>inc/config_overview</tvh_include>

!['Image cache'](static/img/doc/config/imagecache.png)

<tvh_include>inc/config_notes</tvh_include>

---

## Automatic download throttling

When **Automatic download throttling** is enabled (the default), Tvheadend paces
image downloads so the provider does not rate-limit or block the server: it learns
how many images the provider allows per time window and locks onto a sustainable
rate. Images are fetched **soonest-airing first**, so the artwork for what is on now
appears before far-future days. Once locked it keeps probing for a faster pace the
provider will still tolerate, so the cache fills as quickly as the provider allows;
the optimized rate is remembered across restarts. The two fields below then show,
read-only, the delay and pause it has settled on; uncheck the option to set them
yourself (0/0 = no throttling).

The **Reset auto-throttle** button (see below) discards the learned rate and
re-learns it from scratch -- useful after changing provider or network. Progress is
written to the log, including a line when the cache has caught up.

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---
