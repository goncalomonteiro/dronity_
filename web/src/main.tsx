import React from 'react'
import ReactDOM from 'react-dom/client'

export function hello(name: string): string {
  return `Hello, ${name}`
}

if (typeof document !== 'undefined') {
  const root = document.getElementById('root')
  if (root) {
    ReactDOM.createRoot(root).render(<div>Verity Web Stub</div>)
  }
}

