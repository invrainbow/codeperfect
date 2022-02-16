// @ts-check
// Note: type annotations allow type checking and IDEs autocompletion

const lightCodeTheme = require("prism-react-renderer/themes/github");

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: "CodePerfect 95 Docs",
  tagline: "Documentation for CodePerfect 95",
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
        title: "CodePerfect 95 Docs",
        logo: {
          alt: "CodePerfect 95",
          src: "logo.png",
        },
        items: [
          {
            label: "Website",
            position: "right",
            href: "https://codeperfect95.com",
          },
          {
            label: "Download for Mac",
            position: "right",
            href: "https://codeperfect95.com/download",
          },
        ],
      },
      footer: {
        style: "light",
        logo: {
          alt: "CodePerfect 95",
          src: "logo.png",
          href: "https://codeperfect95.com",
          width: 48,
          height: 48,
        },
        copyright: `© ${new Date().getFullYear()} CodePerfect 95`,
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
