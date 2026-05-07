<script lang="ts">
/*
 * KodiText — renders a string with Kodi label-formatting codes
 * (`[B]…[/B]`, `[I]…[/I]`, `[COLOR red]…[/COLOR]`, `[CR]`,
 * `[UPPERCASE]…[/…]`, `[LOWERCASE]…[/…]`, `[CAPITALIZE]…[/…]`)
 * via a render function — no `v-html`, no string-HTML round-trip,
 * no XSS surface. Parser lives in `views/epg/kodiText.ts`.
 *
 * Props:
 *   text     — input string. Empty / undefined → renders nothing.
 *   enabled  — when false, the text renders raw (codes visible).
 *              Caller wires this to `access.data.label_formatting`
 *              so the server-side feature flag controls the parser
 *              the same way the legacy ExtJS UI does.
 *
 * Output is wrapped in a single `<span>` so the surrounding parent's
 * `white-space: pre-wrap` and font-size constraints still apply.
 */
import { defineComponent, h } from 'vue'
import { parseKodiText } from '@/views/epg/kodiText'

export default defineComponent({
  name: 'KodiText',
  props: {
    text: { type: String, default: '' },
    enabled: { type: Boolean, default: true },
  },
  setup(props) {
    return () => {
      const t = props.text
      if (!t) return null
      const children = props.enabled ? parseKodiText(t) : [t]
      return h('span', null, children)
    }
  },
})
</script>
