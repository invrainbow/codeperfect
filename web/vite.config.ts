import { defineConfig } from "vite";
import { resolve } from "path";
import preact from '@preact/preset-vite'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [preact()],
  resolve: {
    alias: {
      "@": resolve(__dirname, "src"),
      "#root": resolve(__dirname),
    },
  },
});
