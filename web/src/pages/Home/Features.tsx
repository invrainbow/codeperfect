import { A } from "@/components/A";
import { Icon } from "@/components/Icon";
import { links } from "@/links";
import { SiVim } from "@react-icons/all-files/si/SiVim";
import {
  IconBinaryTree,
  IconBrain,
  IconBulb,
  IconChevronRight,
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
import cx from "classnames";
import _ from "lodash";

const FEATURES = [
  {
    name: "Autocomplete",
    icon: IconBrain,
    href: "https://docs.codeperfect95.com/automatic-completion",
  },
  {
    name: "Command Palette",
    icon: IconCommand,
    href: "https://docs.codeperfect95.com/command-palette",
  },
  {
    name: "Global Fuzzy Finder",
    icon: IconTelescope,
    href: "https://docs.codeperfect95.com/go-to-file",
  },
  {
    name: "Format File",
    icon: IconWand,
    href: "https://docs.codeperfect95.com/format-file",
  },
  {
    name: "Postfix Completion",
    icon: IconRobot,
    href: "https://docs.codeperfect95.com/postfix-completion",
  },
  {
    name: "Jump to Definition",
    icon: IconBulb,
    href: "https://docs.codeperfect95.com/jump-to-definition",
  },
  {
    name: "Manage Interfaces",
    icon: IconHexagons,
    href: "https://docs.codeperfect95.com/find-interfaces",
  },
  {
    name: "Build & Debug",
    icon: IconTools,
    href: "https://docs.codeperfect95.com/setting-up-builds",
  },
  {
    name: "Global Live Search",
    icon: IconSearch,
    href: "https://docs.codeperfect95.com/search-and-replace",
  },
  {
    name: "Rename Anything",
    icon: IconEdit,
    href: "https://docs.codeperfect95.com/rename",
  },
  {
    name: "Tree-Based Navigation",
    icon: IconBinaryTree,
    href: "https://docs.codeperfect95.com/tree-based-navigation",
  },
  {
    name: "Vim Keybindings",
    icon: SiVim,
    href: "https://docs.codeperfect95.com/vim-keybindings",
  },
  {
    name: "Generate Function",
    icon: IconDiamond,
    href: "https://docs.codeperfect95.com/generate-function",
  },
  {
    name: "Find References",
    icon: IconTags,
    href: "https://docs.codeperfect95.com/find-references",
  },
  {
    name: "Manage Struct Tags",
    icon: IconTags,
    href: "https://docs.codeperfect95.com/struct-tags",
  },
];

const SORTED_FEATURES = _.sortBy(FEATURES, "name");

export const Features = () => (
  <div className="max-w-screen-lg px-4 mx-auto flex flex-col md:flex-row gap-8 items-start md:items-center">
    <div className="md:w-1/3">
      <h1 className="title text-3xl md:text-4xl">
        <div>Batteries included,</div>
        <div>zero configuration.</div>
      </h1>
      <div className="max-w-screen-sm mx-auto mt-4 mb-6 text-lg whitespace-nowrap">
        <p>The speed of Vim, the power of an IDE.</p>
      </div>
      <A
        href={links.docs}
        className="btn btn1 justify-center flex md:inline-flex text-center group"
      >
        View Docs
        <Icon
          size={18}
          className="ml-1.5 relative top-[1px] group-hover:translate-x-1 transition"
          icon={IconChevronRight}
        />
      </A>
    </div>

    <div className="md:mx-0 flex flex-wrap items-start md:grid md:grid-flow-col md:grid-rows-6 gap-2 md:gap-y-2.5 md:gap-x-4 text-xs md:text-sm rounded md:p-6 font-medium md:font-semibold">
      {SORTED_FEATURES.map(({ name, icon, href }) => (
        <div key={name}>
          <A
            className={cx(
              "bg-neutral-100 text-neutral-400 transition-all rounded-lg py-1 px-1.5 md:py-1.5 md:px-2",
              "select-none whitespace-nowrap inline-flex items-center gap-1 flex-shrink leading-none",
              "hover:bg-neutral-200 hover:text-neutral-800 no-underline hover:rotate-2"
            )}
            href={href}
          >
            <Icon
              icon={icon}
              className="w-4 h-4 md:w-5 md:h-5 relative opacity-70"
            />
            <div>{name}</div>
          </A>
        </div>
      ))}
    </div>
  </div>
);
