/*
 * IntersectionObserver test mock.
 *
 * happy-dom (the vitest environment) doesn't implement
 * IntersectionObserver. Tests that exercise composables /
 * components using IO need a stub registered as
 * `globalThis.IntersectionObserver` before the unit-under-test
 * runs.
 *
 * This mock tracks every constructed instance + its observed
 * elements + its callback, and exposes `simulate(entries)` so
 * a test can trigger the observer's callback with synthesised
 * intersection state for chosen elements.
 *
 * Usage:
 *
 *   import { MockIntersectionObserver, installIntersectionObserverMock }
 *     from '@/test/__helpers__/intersectionObserverMock'
 *
 *   beforeEach(() => {
 *     installIntersectionObserverMock()
 *     MockIntersectionObserver.reset()
 *   })
 *
 *   it('reacts to visibility change', () => {
 *     // … bind an element that the unit-under-test observes …
 *     const io = MockIntersectionObserver.lastInstance()!
 *     io.simulate([{ target: el, isIntersecting: true }])
 *     // assert post-state
 *   })
 */

interface SimulatedEntry {
  target: Element
  isIntersecting: boolean
}

export class MockIntersectionObserver {
  static readonly instances: MockIntersectionObserver[] = []

  callback: IntersectionObserverCallback
  options: IntersectionObserverInit
  observed = new Set<Element>()

  constructor(callback: IntersectionObserverCallback, options?: IntersectionObserverInit) {
    this.callback = callback
    this.options = options ?? {}
    MockIntersectionObserver.instances.push(this)
  }

  observe(el: Element): void {
    this.observed.add(el)
  }

  unobserve(el: Element): void {
    this.observed.delete(el)
  }

  disconnect(): void {
    this.observed.clear()
  }

  /* Test helper: invoke the callback with synthesised entries.
   * Each entry is converted to a minimal IntersectionObserverEntry
   * shape — most consumers only read `target` and `isIntersecting`. */
  simulate(entries: SimulatedEntry[]): void {
    const ioEntries = entries.map((e) => ({
      target: e.target,
      isIntersecting: e.isIntersecting,
      intersectionRatio: e.isIntersecting ? 1 : 0,
      boundingClientRect: e.target.getBoundingClientRect(),
      intersectionRect: e.isIntersecting
        ? e.target.getBoundingClientRect()
        : (new DOMRect() as unknown as DOMRectReadOnly),
      rootBounds: null,
      time: 0,
    })) as unknown as IntersectionObserverEntry[]
    this.callback(ioEntries, this as unknown as IntersectionObserver)
  }

  static reset(): void {
    MockIntersectionObserver.instances.length = 0
  }

  static lastInstance(): MockIntersectionObserver | undefined {
    return MockIntersectionObserver.instances[MockIntersectionObserver.instances.length - 1]
  }
}

export function installIntersectionObserverMock(): void {
  ;(globalThis as { IntersectionObserver?: unknown }).IntersectionObserver =
    MockIntersectionObserver
}
