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
        { type: "doc", id: "changelog/0.6", label: "Version 0.6" },
        { type: "doc", id: "changelog/0.5", label: "Version 0.5" },
        { type: "doc", id: "changelog/0.4", label: "Version 0.4" },
        { type: "doc", id: "changelog/0.3", label: "Version 0.3" },
      ],
    },
  ],

  tutorialSidebar: [
    {
      type: "category",
      label: "Overview",
      items: ["getting-started", "getting-license"],
    },
    {
      type: "category",
      label: "Navigation",
      items: ["command-palette", "go-to-file", "go-to-symbol"],
    },
    {
      type: "category",
      label: "Editor",
      items: ["panes-and-tabs", "vim-keybindings", "format-document"],
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
      items: ["rename", "generate-implementation"],
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
