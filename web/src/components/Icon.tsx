import { TablerIconsProps } from "@tabler/icons-react";

interface Props extends TablerIconsProps {
  block?: boolean;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  icon: any;
}

export const Icon = ({ block, icon: IconComponent, ...props }: Props) => (
  <span className={block ? "block" : "inline-block"}>
    <IconComponent {...props} />
  </span>
);
