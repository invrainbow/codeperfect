import { WrappedProps, wrapElement } from "./dom";

import {
  IconBinaryTree,
  IconBrain,
  IconBulb,
  IconChevronRight,
  IconCommand,
  IconDiamond,
  IconDownload,
  IconEdit,
  IconHexagons,
  IconKeyboard,
  IconMenu2,
  IconRobot,
  IconSearch,
  IconTags,
  IconTelescope,
  IconTools,
  Icon as IconType,
  IconWand,
  IconX,
} from "@tabler/icons-react";

const ICONS: Record<string, IconType> = {
  ChevronRight: IconChevronRight,
  Download: IconDownload,
  Menu2: IconMenu2,
  X: IconX,
  Brain: IconBrain,
  Command: IconCommand,
  Telescope: IconTelescope,
  Wand: IconWand,
  Robot: IconRobot,
  Bulb: IconBulb,
  Hexagons: IconHexagons,
  Tools: IconTools,
  Search: IconSearch,
  Edit: IconEdit,
  BinaryTree: IconBinaryTree,
  Keyboard: IconKeyboard,
  Diamond: IconDiamond,
  Tags: IconTags,
} as const;

export type IconName = keyof typeof ICONS;

interface Props extends WrappedProps<IconType> {
  icon: IconName;
}

export const Icon = ({ icon, ...props }: Props) => {
  return wrapElement(ICONS[icon], props);
};
