import { SiVim } from "@react-icons/all-files/si/SiVim";
import {
  IconBinaryTree,
  IconBrain,
  IconBulb,
  IconCommand,
  IconDiamond,
  IconEdit,
  IconHexagons,
  IconRobot,
  IconSearch,
  IconTags,
  IconTelescope,
  IconTools,
  IconWand,
} from "@tabler/icons-react";
import _ from "lodash";

export const SUPPORT_EMAIL = "support@codeperfect95.com";

export const LINKS = {
  docs: "https://docs.codeperfect95.com",
  changelog: "https://docs.codeperfect95.com/changelog",
  nonPessimized: "https://www.youtube.com/watch?v=pgoetgxecw8",
  oldSoftwareOpenedInstantly:
    "https://www.youtube.com/watch?v=GC-0tCy4P1U&t=2168s",
  github: "https://github.com/codeperfect95/codeperfect",
  download: "https://github.com/codeperfect95/codeperfect/releases/latest",
};

const UNSORTED_FEATURE_LIST = [
  {
    name: "Autocomplete",
    body: (
      <>
        <p>
          <img src="/feature-autocomplete.png" alt="autocomplete" />
        </p>
        <p>
          Like any IDE, CodePerfect provides automatic completion to help you
          write code. Press <code>Ctrl+Space</code> to show all declared
          identifiers in the current scope, or press <code>.</code> after an
          identifier to see fields and methods.
        </p>
      </>
    ),
    icon: IconBrain,
  },
  {
    name: "Command Palette",
    body: (
      <>
        <p>
          <img src="/feature-command-palette.png" alt="command palette" />
        </p>
        <p>
          Every action in CodePerfect can be run as a command through the
          command palette. Just press <code>Cmd+K</code> anywhere to bring it
          up.
        </p>
        <p>
          The command palette supports fuzzy search so you can access commands
          quickly. It also displays the keyboard shortcut for future ease of
          use.
        </p>
      </>
    ),
    icon: IconCommand,
  },
  {
    name: "Global Fuzzy Finder",
    body: (
      <>
        <p>
          <img src="/feature-goto-file.png" alt="goto file" />
        </p>
        <p>
          Use fuzzy search to jump to any file or symbol. Press{" "}
          <code>Cmd+P</code> any time to open the file finder, and{" "}
          <code>Cmd+T</code> to open the symbol finder.
        </p>
        <p>
          We know navigation is a critical part of the developer workflow, so we
          specially made sure it was lag-free. From opening the finder, to
          typing each key, to selecting a file, there's no latency at any step.
        </p>
      </>
    ),
    icon: IconTelescope,
  },
  {
    name: "Format File",
    body: (
      <>
        <p>
          CodePerfect allows you to format your file at any time, as well as the
          option to automatically format on save.
        </p>
        <p>
          We use our in-memory index to organize your imports, removing unused
          imports and detecting which imports are needed to resolve undeclared
          symbols. This yields greater accuracy and speed.
        </p>
      </>
    ),
    icon: IconWand,
  },
  {
    name: "Postfix Completion",
    body: (
      <>
        <p>
          <img src="/feature-postfix.png" alt="postfix" />
        </p>
        <p>
          CodePerfect provides several macros in the autocomplete menu after you
          type <code>.</code> after an expression.
        </p>
        <p>
          These macros range from the powerful <code>.check!</code>, which
          assigns variables to the expression's return values, checks whether{" "}
          <code>err != nil</code>, and returns the zero-value of the current
          function's return type if so; to the straightforward{" "}
          <code>.ifnotnil</code>, which expands <code>x.ifnotnil!</code> to
        </p>
        <pre>
          if x != nil &#123;{"\n  "}
          {"// "}cursor here{"\n"}&#125;
        </pre>
      </>
    ),
    icon: IconRobot,
  },
  {
    name: "Jump to Definition",
    body: (
      <>
        <p>
          CodePerfect can jump to the definition of any declared symbol. Either
          move your cursor over it and press <code>Cmd+G</code> (or{" "}
          <code>gd</code> in Vim mode), or hold down <code>Cmd</code> and click
          it.
        </p>
      </>
    ),
    icon: IconBulb,
  },
  {
    name: "Manage Interfaces",
    body: (
      <>
        <p>
          <img
            src="/feature-generate-implementation.png"
            alt="generate implementation"
          />
        </p>
        <p>
          CodePerfect provides several facilities for navigating and working
          with interfaces:
        </p>
        <ul>
          <li>Given a type, list the interfaces it implements</li>
          <li>Given an interface, list the types that implement it</li>
          <li>Generate implementation of interface for type</li>
        </ul>
      </>
    ),
    icon: IconHexagons,
  },
  {
    name: "Build & Debug",
    body: (
      <>
        <p>
          CodePerfect is designed to make the edit-build-debug loop as seamless
          as possible.
        </p>
        <p>
          Trigger a build, jump to the first error, write a fix, jump to next
          error, fix, rebuild. Everything is done with ergonomic hotkeys. Error
          positions are preserved as you edit code. The entire experience is
          frictionless.
        </p>
        <p>
          When you're ready to debug, CodePerfect integrates with Delve to
          provide a seamless debugging experience.
        </p>
      </>
    ),
    icon: IconTools,
  },
  {
    name: "Global Live Search",
    body: (
      <>
        <p>
          Press <code>Cmd+Shift+F</code> to open project-wide search. Grepping a
          whole folder is fast enough on modern machines that we display results
          in realtime after each keystroke. It's like a faster, completely-local
          Algolia.
        </p>
        <p>
          When you see the result you want, use <code>Up</code> and{" "}
          <code>Down</code> to navigate results and <code>Enter</code> to
          select.
        </p>
        <p>
          We also support project-wide replace in the same window; press{" "}
          <code>Cmd+Shift+H</code>.
        </p>
      </>
    ),
    icon: IconSearch,
  },
  {
    name: "Rename Anything",
    body: (
      <>
        <p>
          CodePerfect can rename almost anything that's declared within your
          project. Just hover over an identifier and run the <code>Rename</code>{" "}
          command (or press <code>F12</code>). This works on any identifier, not
          just the actual declaration. It also works on struct field names.
        </p>
      </>
    ),
    icon: IconEdit,
  },
  {
    name: "Tree-Based Navigation",
    body: (
      <>
        <p>
          You can traverse your code by traversing its AST. Press{" "}
          <code>Ctrl+Alt+A</code> to enter tree-based navigation. Then,
        </p>
        <ul>
          <li>
            <code>Down</code> or <code>Right</code> to move to next sibling node
          </li>
          <li>
            <code>Up</code> or <code>Left</code> to move to previous sibling
            node
          </li>
          <li>
            <code>Shift+Down</code> or <code>Shift+Right</code> to move inward
            to child node
          </li>
          <li>
            <code>Shift+Up</code> or <code>Shift+Left</code> to move outward to
            parent node
          </li>
        </ul>
        <p>
          For Vim users, <code>h</code> <code>j</code> <code>k</code>{" "}
          <code>l</code> can be used instead of the arrow keys.
        </p>
      </>
    ),
    icon: IconBinaryTree,
  },
  {
    name: "Vim Keybindings",
    body: (
      <>
        <p>
          CodePerfect supports Vim keybindings out of the box. Go to{" "}
          <code>Tools</code> &gt; <code>Options</code> &gt;{" "}
          <code>Editor Settings</code> to enable them.
        </p>
        <p>
          Vim keybindings are a work in progress, but we currently support most
          of the commonly used commands, and the development team currently uses
          it for everyday text editing.
        </p>
      </>
    ),
    icon: SiVim,
  },
  {
    name: "Video Game Performance",
    body: (
      <>
        <p>
          CodePerfect was designed from the ground up with performance in mind.
          Instant startup, no latency, 144 frames a second, runs as smooth as a
          video game.
        </p>
      </>
    ),
  },
  {
    name: "Generate Function",
    body: (
      <>
        <p>
          Sometimes you're coding and you wish that a function or method
          existed, so you make a note to implement it later.
        </p>
        <p>
          With CodePerfect, you can write a function call, and CodePerfect can
          generate the function signature based on the types of the parameters.
          For instance, if you have
        </p>
        <pre>
          x := 0{"\n"}y := false{"\n"}calculate(x, y)
        </pre>
        <p>
          you can hover over <code>calculate</code> and run{" "}
          <code>Generate Function From Call</code>. It will generate
        </p>
        <pre>func calculate(v0 int, v1 bool)</pre>
      </>
    ),
    icon: IconDiamond,
  },
  {
    name: "Find References",
    body: (
      <>
        <p>
          <img src="/feature-find-references.png" alt="find references" />
        </p>
        <p>
          Find References works as it does in other IDEs. Hover over the name
          and press <code>Cmd+Alt+R</code> or run <code>Find References</code>{" "}
          in the command palette.
        </p>
      </>
    ),
    icon: IconTags,
  },
  {
    name: "Manage Struct Tags",
    body: (
      <>
        <p>
          CodePerfect comes with commands to let you generate, add and remove
          tags from structs. Just search the command palette for{" "}
          <code>struct</code>.
        </p>
      </>
    ),
    icon: IconTags,
  },
];

export const FEATURE_LIST = _.sortBy(UNSORTED_FEATURE_LIST, "name");
