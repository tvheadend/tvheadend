/*
 * ApiError — thrown by apiCall() when the server returns a non-2xx status.
 * Captures status, statusText, and the response body for diagnostics.
 *
 * Callers currently either handle these locally or let them propagate
 * to the console; once a toast/notification surface lands, a global
 * interceptor can catch them and present them to the user.
 */
export class ApiError extends Error {
  constructor(
    public status: number,
    public statusText: string,
    public body?: string
  ) {
    super(`API ${status} ${statusText}`)
    this.name = 'ApiError'
  }
}
