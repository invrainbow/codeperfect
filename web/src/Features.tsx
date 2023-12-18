import { Icon, IconName } from "@/components/Icon";
import { links } from "@/links";
import cx from "classnames";
import { alphabetical } from "radash";
import { Button } from "./components/Button";
import { Link } from "./components/Link";
import dom from "./components/dom";

interface Feature {
  name: string;
  href: string;
  icon: IconName;
}

const FEATURES: Feature[] = [
  {
    name: "Autocomplete",
    icon: "Brain",
    href: "https://docs.codeperfect95.com/automatic-completion",
  },
  {
    name: "Command Palette",
    icon: "Command",
    href: "https://docs.codeperfect95.com/command-palette",
  },
  {
    name: "Global Fuzzy Finder",
    icon: "Telescope",
    href: "https://docs.codeperfect95.com/go-to-file",
  },
  {
    name: "Format File",
    icon: "Wand",
    href: "https://docs.codeperfect95.com/format-file",
  },
  {
    name: "Postfix Completion",
    icon: "Robot",
    href: "https://docs.codeperfect95.com/postfix-completion",
  },
  {
    name: "Jump to Definition",
    icon: "Bulb",
    href: "https://docs.codeperfect95.com/jump-to-definition",
  },
  {
    name: "Manage Interfaces",
    icon: "Hexagons",
    href: "https://docs.codeperfect95.com/find-interfaces",
  },
  {
    name: "Build & Debug",
    icon: "Tools",
    href: "https://docs.codeperfect95.com/setting-up-builds",
  },
  {
    name: "Global Live Search",
    icon: "Search",
    href: "https://docs.codeperfect95.com/search-and-replace",
  },
  {
    name: "Rename Anything",
    icon: "Edit",
    href: "https://docs.codeperfect95.com/rename",
  },
  {
    name: "Tree-Based Navigation",
    icon: "BinaryTree",
    href: "https://docs.codeperfect95.com/tree-based-navigation",
  },
  {
    name: "Vim Keybindings",
    icon: "Keyboard",
    href: "https://docs.codeperfect95.com/vim-keybindings",
  },
  {
    name: "Generate Function",
    icon: "Diamond",
    href: "https://docs.codeperfect95.com/generate-function",
  },
  {
    name: "Find References",
    icon: "Tags",
    href: "https://docs.codeperfect95.com/find-references",
  },
  {
    name: "Manage Struct Tags",
    icon: "Tags",
    href: "https://docs.codeperfect95.com/struct-tags",
  },
];

const SORTED_FEATURES = alphabetical(FEATURES, (it) => it.name);

export const Features = () => (
  <dom.div cx="flex max-w-screen-lg px-4 mx-auto flex-col md:flex-row gap-8 items-start md:items-center">
    <dom.div cx="md:w-1/3">
      <dom.div cx="font-bold text-black tracking-tight text-3xl md:text-4xl">
        <dom.div>Batteries included,</dom.div>
        <dom.div>zero configuration.</dom.div>
      </dom.div>
      <dom.div cx="max-w-screen-sm mx-auto mt-4 mb-6 text-lg whitespace-nowrap">
        The speed of Vim, the power of an IDE.
      </dom.div>
      <Button href={links.docs} cx="flex md:inline-flex">
        View Docs
        <Icon
          size={18}
          cx="relative group-hover:translate-x-1 transition"
          icon="ChevronRight"
        />
      </Button>
    </dom.div>
    <dom.div cx="flex md:mx-0 flex-wrap items-start md:grid md:grid-flow-col md:grid-rows-6 gap-2 md:gap-y-2.5 md:gap-x-4 text-xs md:text-sm rounded md:p-6 font-medium md:font-semibold">
      {SORTED_FEATURES.map(({ name, icon, href }) => (
        <dom.div key={name}>
          <Link
            cx={cx(
              "bg-neutral-100 text-neutral-400 transition-all rounded-lg py-1 px-1.5 md:py-1.5 md:px-2",
              "select-none whitespace-nowrap inline-flex items-center gap-1 flex-shrink leading-none",
              "hover:bg-neutral-200 hover:text-neutral-800 no-underline hover:rotate-2"
            )}
            href={href}
          >
            <Icon icon={icon} cx="w-4 h-4 md:w-5 md:h-5 relative opacity-70" />
            <dom.div>{name}</dom.div>
          </Link>
        </dom.div>
      ))}
    </dom.div>
  </dom.div>
);
