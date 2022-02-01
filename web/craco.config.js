const CracoEsbuildPlugin = require("craco-esbuild");

module.exports = {
  plugins: [
    { plugin: CracoEsbuildPlugin },
    {
      plugin: {
        overrideWebpackConfig: ({ webpackConfig }) => {
          webpackConfig.resolve.extensions = [".mjs", ".js", ".jsx", ".json"];
          return webpackConfig;
        },
      },
    },
  ],
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
