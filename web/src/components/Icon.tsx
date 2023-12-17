import { Box, Props as BoxProps } from "./Box";

import {
  IconChevronRight,
  IconDownload,
  IconMenu2,
  IconX,
  IconBrain,
  IconCommand,
  IconTelescope,
  IconWand,
  IconRobot,
  IconBulb,
  IconHexagons,
  IconTools,
  IconSearch,
  IconEdit,
  IconBinaryTree,
  IconKeyboard,
  IconDiamond,
  IconTags,
} from "@tabler/icons-react";

const ICONS = {
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

interface Props extends BoxProps<(typeof ICONS)[IconName]> {
  icon: IconName;
}

export const Icon = ({ icon, ...props }: Props) => (
  <Box as={ICONS[icon]} {...props} />
);
