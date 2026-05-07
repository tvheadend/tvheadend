import { defineConfigWithVueTs, vueTsConfigs } from '@vue/eslint-config-typescript'
import pluginVue from 'eslint-plugin-vue'
import eslintConfigPrettier from 'eslint-config-prettier'

export default defineConfigWithVueTs(
  {
    /*
     * src/views/_dev/** is the local-only playground (gitignored, see
     * static-vue/.gitignore). Files there are throwaway prototypes and
     * shouldn't have to pass lint — they're for trying things out, not
     * shipping. Production routes never include them (see router/index.ts
     * dev-route mechanism guarded by import.meta.env.DEV).
     */
    ignores: ['dist/**', 'node_modules/**', 'src/views/_dev/**'],
  },
  pluginVue.configs['flat/recommended'],
  vueTsConfigs.recommended,
  eslintConfigPrettier,
  {
    rules: {
      'vue/multi-word-component-names': 'off',
    },
  }
)
