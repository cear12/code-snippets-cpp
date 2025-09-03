std::shared_ptr<X> px(std::make_shared<X>());
std::shared_ptr<Y> py(px,&px->y);