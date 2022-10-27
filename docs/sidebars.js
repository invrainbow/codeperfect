/**
 * Creating a sidebar enables you to:
 - create an ordered group of docs
 - render a sidebar for each doc of that group
 - provide next/previous navigation

 The sidebars can be generated from the filesystem, or explicitly defined here.

 Create as many sidebars as you want.
 */

// @ts-check

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  changelogSidebar: [
    {
      type: "category",
      label: "Changelog",
      items: [
        "changelog/22.10.2",
        "changelog/22.10.1",
        "changelog/22.10",
        "changelog/22.09.7",
        "changelog/22.09.6",
        "changelog/22.09.5",
        "changelog/22.09.4",
        "changelog/22.09.3",
        "changelog/22.09.2",
        "changelog/22.09.1",
        "changelog/22.09",
      ],
    },
  ],

  tutorialSidebar: [
    {
      type: "category",
      label: "Overview",
      items: ["getting-started", "getting-license", "platform-differences"],
    },
    {
      type: "category",
      label: "Navigation",
      items: [
        "command-palette",
        "go-to-file",
        "go-to-symbol",
        "history",
        "tree-based-navigation",
      ],
    },
    {
      type: "category",
      label: "Editor",
      items: [
        "panes-and-tabs",
        "vim-keybindings",
        "format-document",
        "toggle-comment",
      ],
    },
    {
      type: "category",
      label: "Code Intelligence",
      items: [
        "automatic-completion",
        "jump-to-definition",
        "parameter-hints",
        "postfix-completion",
        "find-interfaces",
        "find-implementations",
        "find-references",
      ],
    },
    {
      type: "category",
      label: "Refactoring",
      items: [
        "rename",
        "generate-implementation",
        "generate-function",
        "struct-tags",
      ],
    },
    {
      type: "category",
      label: "Building",
      items: ["setting-up-builds", "start-a-build", "navigating-errors"],
    },
    {
      type: "category",
      label: "Debugging",
      items: [
        "setting-up-debugger",
        "start-a-debug-session",
        "breakpoints",
        "step-through-program",
        "examine-program-state",
        "debug-a-test",
      ],
    },
  ],
};

module.exports = sidebars;
