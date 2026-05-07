/*
 * useToastNotify — thin wrapper over PrimeVue's `useToast()` injection
 * that bakes our error-toast convention into a single `error()` call.
 *
 * Replaces the scattered `globalThis.alert(\`${prefix}: ${msg}\`)`
 * pattern that surfaced action failures (DVR cancel/stop/delete,
 * EPG record/stop/delete, status-view clear-stats). The native
 * alert() popup is modal, blocks the page, and ignores the theme;
 * Toast is dismissable, non-blocking, and follows Aura tokens.
 *
 * Severity / lifetime defaults match common admin-UI conventions:
 *   - error: stays until dismissed (life: 0). User has time to read
 *     the failure detail and decide what to do.
 *   - warn: 8 s. Time-bounded but readable.
 *   - success: 3 s. Quick acknowledgement; non-modal.
 *   - info: 5 s. Mid-range.
 *
 * Must be called from a Vue setup context — `useToast()` reads from
 * the current instance's inject tree, populated by
 * `app.use(ToastService)` in main.ts.
 */
import { useToast as primeUseToast } from 'primevue/usetoast'

export interface ToastOptions {
  /** Header line — defaults vary by severity (see error / warn / etc.). */
  summary?: string
  /** Lifetime in milliseconds; 0 means "until dismissed". */
  life?: number
}

export function useToastNotify() {
  const toast = primeUseToast()

  function error(detail: string, opts: ToastOptions = {}) {
    toast.add({
      severity: 'error',
      summary: opts.summary ?? 'Error',
      detail,
      life: opts.life ?? 0,
    })
  }

  function warn(detail: string, opts: ToastOptions = {}) {
    toast.add({
      severity: 'warn',
      summary: opts.summary ?? 'Warning',
      detail,
      life: opts.life ?? 8000,
    })
  }

  function success(detail: string, opts: ToastOptions = {}) {
    toast.add({
      severity: 'success',
      summary: opts.summary ?? 'Done',
      detail,
      life: opts.life ?? 3000,
    })
  }

  function info(detail: string, opts: ToastOptions = {}) {
    toast.add({
      severity: 'info',
      summary: opts.summary ?? 'Info',
      detail,
      life: opts.life ?? 5000,
    })
  }

  return { error, warn, success, info }
}
