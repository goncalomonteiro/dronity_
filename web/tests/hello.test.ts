import { describe, it, expect } from 'vitest'
import { hello } from '../src/main'

describe('hello', () => {
  it('greets by name', () => {
    expect(hello('World')).toBe('Hello, World')
  })
})

