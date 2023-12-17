import { ElementType } from "react";
import { Box, Props as BoxProps } from "./Box";

export type Props<E extends ElementType = "div"> = BoxProps<E>;

export const Flex = <E extends ElementType = "div">({
  cx,
  as,
  ...rest
}: Props<E>) => (
  <Box as={as ?? "div"} cx={["flex items-center", cx]} {...rest} />
);
