/**
 * Creating a sidebar enables you to:
 - create an ordered group of docs
 - render a sidebar for each doc of that group
 - provide next/previous navigation

 The sidebars can be generated from the filesystem, or explicitly defined here.

 Create as many sidebars as you want.
 */

// @ts-check

const fs = require("fs");

function isVersion(s) {
  const parts = s.split(".");
  return (
    (parts.length === 2 || parts.length === 3) &&
    parts.every((it) => !isNaN(it))
  );
}

function parseVersion(s) {
  const parts = s.split(".").map((it) => parseInt(it, 10));
  if (parts.length === 2) {
    parts.push(0);
  }

  let ret = 0;
  parts.forEach((it) => {
    ret *= 100;
    ret += it;
  });
  return ret;
}

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  tutorialSidebar: [
    {
      type: "category",
      label: "Overview",
      items: ["getting-started"],
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
        "search-and-replace",
      ],
    },
    {
      type: "category",
      label: "Editor",
      items: [
        "panes-and-tabs",
        "vim-keybindings",
        "format-file",
        "toggle-comment",
        "find-and-replace",
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
