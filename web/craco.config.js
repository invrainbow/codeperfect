const CracoEsbuildPlugin = require("craco-esbuild");

module.exports = {
  plugins: [{ plugin: CracoEsbuildPlugin }],
  style: {
    postcss: {
      plugins: [require("tailwindcss"), require("autoprefixer")],
    },
  },
  webpack: {
    alias: {
      react: "preact/compat",
      "react-dom": "preact/compat",
    },
  },
};
