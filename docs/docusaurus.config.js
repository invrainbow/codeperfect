// @ts-check
// Note: type annotations allow type checking and IDEs autocompletion

const lightCodeTheme = require("prism-react-renderer/themes/github");

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: "CodePerfect Docs",
  tagline: "Documentation for CodePerfect",
  url: "https://docs.codeperfect95.com",
  baseUrl: "/",
  onBrokenLinks: "throw",
  onBrokenMarkdownLinks: "warn",
  favicon: "favicon.png",
  organizationName: "codeperfect",
  projectName: "codeperfect",

  plugins: ["docusaurus-plugin-sass"],

  presets: [
    [
      "classic",
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          routeBasePath: "/",
          sidebarPath: require.resolve("./sidebars.js"),
          sidebarCollapsible: false,
        },
        theme: {
          customCss: require.resolve("./src/css/custom.scss"),
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      navbar: {
        title: "CodePerfect Docs",
        logo: {
          alt: "CodePerfect",
          src: "logo.png",
        },
        items: [
          {
            label: "Download",
            position: "right",
            href: "https://github.com/invrainbow/codeperfect/releases/latest",
          },
          {
            label: "Website",
            position: "right",
            href: "https://codeperfect95.com",
          },
        ],
      },
      footer: {
        style: "dark",
        logo: {
          alt: "CodePerfect",
          src: "logo.png",
          href: "https://codeperfect95.com",
          width: 48,
          height: 48,
        },
        copyright: `© ${new Date().getFullYear()} CodePerfect`,
      },
      prism: {
        theme: lightCodeTheme,
      },
      colorMode: {
        disableSwitch: true,
      },
    }),
};

module.exports = config;
